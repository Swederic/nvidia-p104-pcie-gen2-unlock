// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/pci_regs.h>
#include <linux/delay.h>

#define DOMAIN 0
#define BUS    3
#define DEVFN  PCI_DEVFN(0, 0)

static bool retrain;
module_param(retrain, bool, 0400);
MODULE_PARM_DESC(retrain, "Request one root-port retrain");

static void log_link(const char *where, u16 s)
{
    pr_info("p104train: %-4s STA=%04x speed=%u width=%u training=%u dll=%u\n",
            where, s,
            s & PCI_EXP_LNKSTA_CLS,
            (s & PCI_EXP_LNKSTA_NLW) >> 4,
            !!(s & PCI_EXP_LNKSTA_LT),
            !!(s & PCI_EXP_LNKSTA_DLLLA));
}


static struct pci_dev *find_p104(void)
{
    struct pci_dev *gpu, *other;

    gpu = pci_get_device(0x10de, 0x1b87, NULL);
    if (!gpu)
        return NULL;

    /* The second lookup consumes its starting reference. */
    other = pci_get_device(0x10de, 0x1b87,
                           pci_dev_get(gpu));

    if (other) {
        pr_err("p104train: multiple P104 devices found\n");
        pci_dev_put(other);
        pci_dev_put(gpu);
        return NULL;
    }

    pr_info("p104train: P104 found at %s\n", pci_name(gpu));
    return gpu;
}

static int __init train_init(void)
{
    struct pci_dev *gpu, *rp;
    u16 gs, rs, gt, rt, ctl;
    u16 prev_g, prev_r;
    u32 cap2;
    int i, rc = 0, transitions = 0;
    int training_samples = 0;
    int gen2_samples = 0;
    int dll_down_samples = 0;

    gpu = find_p104();
    if (!gpu)
        return -ENODEV;

    if (gpu->vendor != 0x10de || gpu->device != 0x1b87) {
        rc = -ENODEV;
        goto out;
    }

    rp = pci_upstream_bridge(gpu);

    if (!rp || !pci_is_pcie(rp) ||
        pci_pcie_type(rp) != PCI_EXP_TYPE_ROOT_PORT) {
        pr_err("p104train: unexpected root port\n");
        rc = -ENODEV;
        goto out;
    }

    if (pcie_capability_read_dword(
            gpu, PCI_EXP_LNKCAP2, &cap2) ||
        pcie_capability_read_word(
            gpu, PCI_EXP_LNKSTA, &gs) ||
        pcie_capability_read_word(
            rp, PCI_EXP_LNKSTA, &rs) ||
        pcie_capability_read_word(
            gpu, PCI_EXP_LNKCTL2, &gt) ||
        pcie_capability_read_word(
            rp, PCI_EXP_LNKCTL2, &rt)) {
        rc = -EIO;
        goto out;
    }

    pr_info("p104train: CAP2=%08x GPU_TLS=%u RP_TLS=%u\n",
            cap2, gt & 0xf, rt & 0xf);

    log_link("GPU", gs);
    log_link("ROOT", rs);

    if (!retrain) {
        pr_info("p104train: AUDIT ONLY\n");
        goto out;
    }

    if ((cap2 & 0x6) != 0x6 ||
        (gt & 0xf) != 2 ||
        (rt & 0xf) != 2 ||
        (gs & PCI_EXP_LNKSTA_CLS) != 1 ||
        (rs & PCI_EXP_LNKSTA_CLS) != 1 ||
        (gs & PCI_EXP_LNKSTA_NLW) != (4 << 4) ||
        (rs & PCI_EXP_LNKSTA_NLW) != (4 << 4) ||
        (gs & PCI_EXP_LNKSTA_LT) ||
        (rs & PCI_EXP_LNKSTA_LT)) {
        pr_err("p104train: unexpected initial state, no retrain\n");
        goto out;
    }

    if (pcie_capability_read_word(
            rp, PCI_EXP_LNKCTL, &ctl)) {
        rc = -EIO;
        goto out;
    }

    prev_g = gs;
    prev_r = rs;

    pr_info("p104train: REQUEST ROOT RETRAIN\n");

    rc = pcie_capability_write_word(
        rp, PCI_EXP_LNKCTL, ctl | PCI_EXP_LNKCTL_RL);

    if (rc) {
        pr_err("p104train: retrain request failed %d\n", rc);
        goto out;
    }

    for (i = 0; i < 400; i++) {
        if (pcie_capability_read_word(
                gpu, PCI_EXP_LNKSTA, &gs) ||
            pcie_capability_read_word(
                rp, PCI_EXP_LNKSTA, &rs)) {
            pr_err("p104train: PCI config read failed\n");
            break;
        }

        if (gs == 0xffff || rs == 0xffff) {
            pr_err("p104train: device/link inaccessible\n");
            break;
        }

        if ((gs | rs) & PCI_EXP_LNKSTA_LT)
            training_samples++;

        if ((gs & PCI_EXP_LNKSTA_CLS) == 2)
            gen2_samples++;

        if (!(gs & PCI_EXP_LNKSTA_DLLLA) ||
            !(rs & PCI_EXP_LNKSTA_DLLLA))
            dll_down_samples++;

        if (gs != prev_g || rs != prev_r) {
            pr_info("p104train: sample=%d GPU=%04x ROOT=%04x\n",
                    i, gs, rs);
            prev_g = gs;
            prev_r = rs;
            transitions++;
        }

        usleep_range(5000, 6000);
    }

    pr_info("p104train: RESULT transitions=%d training=%d gen2=%d dll_down=%d samples=%d\n",
            transitions, training_samples,
            gen2_samples, dll_down_samples, i);

    log_link("GPU", gs);
    log_link("ROOT", rs);

out:
    pci_dev_put(gpu);
    return rc;
}

static void __exit train_exit(void)
{
    pr_info("p104train: unloaded\n");
}

module_init(train_init);
module_exit(train_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("P104 PCIe readout and single root-port retrain");
