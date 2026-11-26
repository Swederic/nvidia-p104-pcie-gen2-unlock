// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/pci_regs.h>
#include <linux/io.h>
#include <linux/delay.h>

#define BAR_LEN     0x90000
#define OFF_PTOP    0x2241C
#define OFF_CAP2    0x880A4
#define OFF_PRIV    0x8841C
#define OFF_VSEC    0x88610
#define OFF_LTSSM   0x8872C
#define OFF_SPEED   0x8C040
#define OFF_CTRL2   0x8C1C0
#define OFF_CYA0    0x8C2C0
#define OFF_CYA1    0x8C300

static bool enable_writes;
module_param(enable_writes, bool, 0400);
MODULE_PARM_DESC(enable_writes,
                 "Enable experimental declaration writes");

static u32 rd(void __iomem *bar, u32 off)
{
    return readl((u8 __iomem *)bar + off);
}


static int wr_checked(void __iomem *bar, u32 off,
                      u32 mask, u32 value,
                      const char *name)
{
    u32 old = rd(bar, off);
    u32 next, actual;

    if (old == 0xFFFFFFFFu)
        return -EIO;

    next = (old & ~mask) | value;
    writel(next, (u8 __iomem *)bar + off);
    actual = rd(bar, off);

    pr_info("p104v6: %-7s %08x -> %08x readback=%08x\n",
            name, old, next, actual);

    return actual == next ? 0 : -EIO;
}


/* Locate the one P104 regardless of PCI bus numbering.
 * pci_get_device(from) releases the 'from' reference.
 */
static struct pci_dev *find_p104(void)
{
    struct pci_dev *gpu, *other;

    gpu = pci_get_device(0x10de, 0x1b87, NULL);
    if (!gpu)
        return NULL;

    /* Extra reference is consumed by the second lookup. */
    other = pci_get_device(0x10de, 0x1b87,
                           pci_dev_get(gpu));

    if (other) {
        pr_err("p104-gen2: multiple P104 devices; aborting\n");
        pci_dev_put(other);
        pci_dev_put(gpu);
        return NULL;
    }

    pr_info("p104-gen2: found P104 at %s\n",
            pci_name(gpu));
    return gpu;
}

static int __init p104v5_init(void)
{
    struct pci_dev *gpu;
    void __iomem *bar;
    u32 cap2;
    int i;

    gpu = find_p104();

    if (!gpu)
        return -ENODEV;

    if (gpu->vendor != 0x10de ||
        gpu->device != 0x1b87 ||
        !(pci_resource_flags(gpu, 0) & IORESOURCE_MEM) ||
        pci_resource_len(gpu, 0) < BAR_LEN) {
        pci_dev_put(gpu);
        return -ENODEV;
    }

    bar = pci_iomap(gpu, 0, BAR_LEN);
    if (!bar) {
        pci_dev_put(gpu);
        return -ENOMEM;
    }

    pr_info("p104v6: BEFORE PTOP=%08x PRIV=%08x CAP2=%08x\n",
            rd(bar, OFF_PTOP),
            rd(bar, OFF_PRIV),
            rd(bar, OFF_CAP2));

    pr_info("p104v6: BEFORE LTSSM=%08x CTRL2=%08x VSEC=%08x SPEED=%08x\n",
            rd(bar, OFF_LTSSM),
            rd(bar, OFF_CTRL2),
            rd(bar, OFF_VSEC),
            rd(bar, OFF_SPEED));

    if (!enable_writes) {
        pr_info("p104v6: AUDIT ONLY\n");
        goto out;
    }

    /* Require the documented factory state. */
    if (rd(bar, OFF_PTOP) != 0x00000000 ||
        rd(bar, OFF_PRIV) != 0xC0B46500 ||
        rd(bar, OFF_CAP2) != 0x00000002 ||
        (rd(bar, OFF_LTSSM) != 0x00000000 &&
         rd(bar, OFF_LTSSM) != 0x00000006) ||
        rd(bar, OFF_CTRL2) != 0x00210036 ||
        (rd(bar, OFF_VSEC) != 0x00000000 &&
         rd(bar, OFF_VSEC) != 0x00000001) ||
        rd(bar, OFF_SPEED) != 0x20085800 ||
        (rd(bar, OFF_CYA0) & 4u) != 0 ||
        (rd(bar, OFF_CYA1) & 0x10u) != 0) {
        pr_err("p104v6: unexpected baseline; NO WRITES\n");
        goto out;
    }


    {
        struct pci_dev *up = pci_upstream_bridge(gpu);
        u16 ep_tls, rp_tls, rp_ctl, sta;
        int err = 0;

        /* Verify the specific x99 root port before writing anything. */
        if (!up || !pci_is_pcie(up) ||
            pci_pcie_type(up) != PCI_EXP_TYPE_ROOT_PORT) {
            pr_err("p104v6: unexpected upstream bridge\n");
            goto out;
        }

        /* Phase 1: the previously verified declaration sequence. */
        err = wr_checked(bar, OFF_LTSSM, 0xF, 6, "LTSSM");
        if (err) goto out;

        err = wr_checked(bar, OFF_PTOP, 0x81, 0x81, "PTOP");
        if (err) goto out;

        err = wr_checked(bar, OFF_PRIV, 0x6000, 0, "PRIV");
        if (err) goto out;

        err = wr_checked(bar, OFF_CTRL2, 0xF0000, 0x20000, "CTRL2");
        if (err) goto out;

        err = wr_checked(bar, OFF_VSEC, 0x1001, 1, "VSEC");
        if (err) goto out;

        cap2 = rd(bar, OFF_CAP2);
        pr_info("p104v6: DECLARED CAP2=%08x\n", cap2);

        if (cap2 != 0x00000006u) {
            pr_err("p104v6: Gen2 not declared, stopping\n");
            goto out;
        }

        /* Phase 2: risky runtime SPEED write.
         * One attempt only; no watchdog or retries.
         */
if (rd(bar, OFF_SPEED) != 0x20085800u) {
        pr_info("p104v6: BEGIN SPEED CHANGE\n");
    goto out;
}
        err = wr_checked(bar, OFF_SPEED,
                         0x000C0000u, 0x00040000u,
                         "SPEED");
        if (err) {
            pr_err("p104v6: SPEED write failed\n");
            goto out;
        }

        /* Phase 3: target 5 GT/s on both ends. */
        if (pcie_capability_read_word(
                gpu, PCI_EXP_LNKCTL2, &ep_tls))
            goto out;

        if (pcie_capability_read_word(
                up, PCI_EXP_LNKCTL2, &rp_tls))
            goto out;

        if (pcie_capability_write_word(
                gpu, PCI_EXP_LNKCTL2,
                (ep_tls & ~PCI_EXP_LNKCTL2_TLS) |
                PCI_EXP_LNKCTL2_TLS_5_0GT))
            goto out;

        if (pcie_capability_write_word(
                up, PCI_EXP_LNKCTL2,
                (rp_tls & ~PCI_EXP_LNKCTL2_TLS) |
                PCI_EXP_LNKCTL2_TLS_5_0GT))
            goto out;

        pr_info("p104v6: TLS endpoint/root set to Gen2\n");

        if (pcie_capability_read_word(
                up, PCI_EXP_LNKCTL, &rp_ctl))
            goto out;

        /* Exactly one retrain request. */
        pr_info("p104v6: RETRAIN REQUEST\n");

        if (pcie_capability_write_word(
                up, PCI_EXP_LNKCTL,
                rp_ctl | PCI_EXP_LNKCTL_RL))
            goto out;

        for (i = 0; i < 20; i++) {
            msleep(100);

            if (pcie_capability_read_word(
                    gpu, PCI_EXP_LNKSTA, &sta))
                break;

            if ((sta & PCI_EXP_LNKSTA_CLS) ==
                PCI_EXP_LNKSTA_CLS_5_0GB) {
                pr_info("p104v6: SUCCESS Gen2 x%u at %u ms\n",
                        (sta >> 4) & 0x3F,
                        (i + 1) * 100);
                break;
            }
        }

        pr_info("p104v6: FINAL LINK STATUS=%04x\n", sta);
    }

out:
    pci_iounmap(gpu, bar);
    pci_dev_put(gpu);
    return 0;
}

static void __exit p104v5_exit(void)
{
    pr_info("p104v6: unloaded; registers not restored\n");
}

module_init(p104v5_init);
module_exit(p104v5_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Experimental P104 Gen2 x4 single retrain test");
