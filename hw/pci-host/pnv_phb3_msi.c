/*
 * QEMU PowerPC PowerNV PHB3 model
 *
 * Copyright (c) 2014-2018, IBM Corporation.
 *
 * This code is licensed under the GPL version 2 or later. See the
 * COPYING file in the top-level directory.
 */
#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qapi/error.h"
#include "qemu-common.h"
#include "hw/pci-host/pnv_phb3_regs.h"
#include "hw/pci-host/pnv_phb3.h"
#include "hw/ppc/pnv.h"
#include "hw/pci/msi.h"
#include "monitor/monitor.h"

static uint64_t phb3_msi_ive_addr(PnvPHB3 *phb, int srcno)
{
    uint64_t ivtbar = phb->regs[PHB_IVT_BAR >> 3];
    uint64_t phbctl = phb->regs[PHB_CONTROL >> 3];

    if (!(ivtbar & PHB_IVT_BAR_ENABLE)) {
        qemu_log_mask(LOG_GUEST_ERROR, "Failed access to disable IVT BAR !");
        return 0;
    }

    if (srcno >= (ivtbar & PHB_IVT_LENGTH_MASK)) {
        qemu_log_mask(LOG_GUEST_ERROR, "MSI out of bounds (%d vs  0x%"PRIx64")",
                      srcno, (uint64_t) (ivtbar & PHB_IVT_LENGTH_MASK));
        return 0;
    }

    ivtbar &= PHB_IVT_BASE_ADDRESS_MASK;

    if (phbctl & PHB_CTRL_IVE_128_BYTES) {
        return ivtbar + 128 * srcno;
    } else {
        return ivtbar + 16 * srcno;
    }
}

static bool phb3_msi_read_ive(PnvPHB3 *phb, int srcno, uint64_t *out_ive)
{
    uint64_t ive_addr, ive;

    ive_addr = phb3_msi_ive_addr(phb, srcno);
    if (!ive_addr) {
        return false;
    }

    if (dma_memory_read(&address_space_memory, ive_addr, &ive, sizeof(ive))) {
        qemu_log_mask(LOG_GUEST_ERROR, "Failed to read IVE at 0x%" PRIx64,
                      ive_addr);
        return false;
    }
    *out_ive = be64_to_cpu(ive);

    return true;
}

static void phb3_msi_set_p(Phb3MsiState *msi, int srcno, uint8_t gen)
{
    uint64_t ive_addr;
    uint8_t p = 0x01 | (gen << 1);

    ive_addr = phb3_msi_ive_addr(msi->phb, srcno);
    if (!ive_addr) {
        return;
    }

    if (dma_memory_write(&address_space_memory, ive_addr + 4, &p, 1)) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "Failed to write IVE (set P) at 0x%" PRIx64, ive_addr);
    }
}

static void phb3_msi_set_q(Phb3MsiState *msi, int srcno)
{
    uint64_t ive_addr;
    uint8_t q = 0x01;

    ive_addr = phb3_msi_ive_addr(msi->phb, srcno);
    if (!ive_addr) {
        return;
    }

    if (dma_memory_write(&address_space_memory, ive_addr + 5, &q, 1)) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "Failed to write IVE (set Q) at 0x%" PRIx64, ive_addr);
    }
}

static void phb3_msi_try_send(Phb3MsiState *msi, int srcno, bool force)
{
    ICSState *ics = ICS_BASE(msi);
    uint64_t ive;
    uint64_t server, prio, pq, gen;

    if (!phb3_msi_read_ive(msi->phb, srcno, &ive)) {
        return;
    }

    server = GETFIELD(IODA2_IVT_SERVER, ive);
    prio = GETFIELD(IODA2_IVT_PRIORITY, ive);
    if (!force) {
        pq = GETFIELD(IODA2_IVT_Q, ive) | (GETFIELD(IODA2_IVT_P, ive) << 1);
    } else {
        pq = 0;
    }
    gen = GETFIELD(IODA2_IVT_GEN, ive);

    /*
     * The low order 2 bits are the link pointer (Type II interrupts).
     * Shift back to get a valid IRQ server.
     */
    server >>= 2;

    switch (pq) {
    case 0: /* 00 */
        if (prio == 0xff) {
            /* Masked, set Q */
            phb3_msi_set_q(msi, srcno);
        } else {
            /* Enabled, set P and send */
            phb3_msi_set_p(msi, srcno, gen);
            icp_irq(ics, server, srcno + ics->offset, prio);
        }
        break;
    case 2: /* 10 */
        /* Already pending, set Q */
        phb3_msi_set_q(msi, srcno);
        break;
    case 1: /* 01 */
    case 3: /* 11 */
    default:
        /* Just drop stuff if Q already set */
        break;
    }
}

static void phb3_msi_set_irq(void *opaque, int srcno, int val)
{
    Phb3MsiState *msi = PHB3_MSI(opaque);

    if (val) {
        phb3_msi_try_send(msi, srcno, false);
    }
}


void pnv_phb3_msi_send(Phb3MsiState *msi, uint64_t addr, uint16_t data,
                       int32_t dev_pe)
{
    ICSState *ics = ICS_BASE(msi);
    uint64_t ive;
    uint16_t pe;
    uint32_t src = ((addr >> 4) & 0xffff) | (data & 0x1f);

    if (src >= ics->nr_irqs) {
        qemu_log_mask(LOG_GUEST_ERROR, "MSI %d out of bounds", src);
        return;
    }
    if (dev_pe >= 0) {
        if (!phb3_msi_read_ive(msi->phb, src, &ive)) {
            return;
        }
        pe = GETFIELD(IODA2_IVT_PE, ive);
        if (pe != dev_pe) {
            qemu_log_mask(LOG_GUEST_ERROR,
                          "MSI %d send by PE#%d but assigned to PE#%d",
                          src, dev_pe, pe);
            return;
        }
    }
    qemu_irq_pulse(msi->qirqs[src]);
}

void pnv_phb3_msi_ffi(Phb3MsiState *msi, uint64_t val)
{
    /* Emit interrupt */
    pnv_phb3_msi_send(msi, val, 0, -1);

    /* Clear FFI lock */
    msi->phb->regs[PHB_FFI_LOCK >> 3] = 0;
}

static void phb3_msi_reject(ICSState *ics, uint32_t nr)
{
    Phb3MsiState *msi = PHB3_MSI(ics);
    unsigned int srcno = nr - ics->offset;
    unsigned int idx = srcno >> 6;
    unsigned int bit = 1ull << (srcno & 0x3f);

    assert(srcno < PHB3_MAX_MSI);

    msi->rba[idx] |= bit;
    msi->rba_sum |= (1u << idx);
}

static void phb3_msi_resend(ICSState *ics)
{
    Phb3MsiState *msi = PHB3_MSI(ics);
    unsigned int i, j;

    if (msi->rba_sum == 0) {
        return;
    }

    for (i = 0; i < 32; i++) {
        if ((msi->rba_sum & (1u << i)) == 0) {
            continue;
        }
        msi->rba_sum &= ~(1u << i);
        for (j = 0; j < 64; j++) {
            if ((msi->rba[i] & (1ull << j)) == 0) {
                continue;
            }
            msi->rba[i] &= ~(1u << j);
            phb3_msi_try_send(msi, i * 64 + j, true);
        }
    }
}

static void phb3_msi_reset(DeviceState *dev)
{
    Phb3MsiState *msi = PHB3_MSI(dev);
    ICSStateClass *icsc = ICS_BASE_GET_CLASS(dev);

    icsc->parent_reset(dev);

    memset(msi->rba, 0, sizeof(msi->rba));
    msi->rba_sum = 0;
}

static void phb3_msi_reset_handler(void *dev)
{
    phb3_msi_reset(dev);
}

void pnv_phb3_msi_update_config(Phb3MsiState *msi, uint32_t base,
                                uint32_t count)
{
    ICSState *ics = ICS_BASE(msi);

    if (count > PHB3_MAX_MSI) {
        count = PHB3_MAX_MSI;
    }
    ics->nr_irqs = count;
    ics->offset = base;
}

static void phb3_msi_realize(DeviceState *dev, Error **errp)
{
    Phb3MsiState *msi = PHB3_MSI(dev);
    ICSState *ics = ICS_BASE(msi);
    ICSStateClass *icsc = ICS_BASE_GET_CLASS(ics);
    Object *obj;
    Error *local_err = NULL;

    icsc->parent_realize(dev, &local_err);
    if (local_err) {
        error_propagate(errp, local_err);
        return;
    }

    obj = object_property_get_link(OBJECT(dev), "phb", &local_err);
    if (!obj) {
        error_propagate(errp, local_err);
        error_prepend(errp, "required link 'phb' not found: ");
        return;
    }
    msi->phb = PNV_PHB3(obj);

    msi->qirqs = qemu_allocate_irqs(phb3_msi_set_irq, msi, ics->nr_irqs);

    qemu_register_reset(phb3_msi_reset_handler, dev);
}

static void phb3_msi_instance_init(Object *obj)
{
    ICSState *ics = ICS_BASE(obj);

    /* Will be overriden later */
    ics->offset = 0;
}

static void phb3_msi_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ICSStateClass *isc = ICS_BASE_CLASS(klass);

    device_class_set_parent_realize(dc, phb3_msi_realize,
                                    &isc->parent_realize);
    device_class_set_parent_reset(dc, phb3_msi_reset,
                                  &isc->parent_reset);

    isc->reject = phb3_msi_reject;
    isc->resend = phb3_msi_resend;
}

static const TypeInfo phb3_msi_info = {
    .name = TYPE_PHB3_MSI,
    .parent = TYPE_ICS_BASE,
    .instance_size = sizeof(Phb3MsiState),
    .class_init = phb3_msi_class_init,
    .class_size = sizeof(ICSStateClass),
    .instance_init = phb3_msi_instance_init,
};

static void pnv_phb3_msi_register_types(void)
{
    type_register_static(&phb3_msi_info);
}

type_init(pnv_phb3_msi_register_types)
