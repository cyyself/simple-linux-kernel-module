#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yangyu Chen (cyy@cyyself.name)");
MODULE_DESCRIPTION("Driver for FPGA PCIe Demo");

//#define REQ_REGION
#define REQ_MSI
#define FPD_NR_IRQS 1

const size_t fpd_dma_size = 1024ul*1024;

static const struct pci_device_id fpd_pci_ids[] = {
	{ PCI_DEVICE(0x10EE, 0x7021)},
	{ 0, },
};
MODULE_DEVICE_TABLE(pci, fpd_pci_ids);

struct fpd_driver_obj {
	bool dma_enabled;
	dma_addr_t device_dma_addr;
	void *cpu_addr;
	int irq_num[FPD_NR_IRQS];
};

int fpd_setup_dma(struct pci_dev *dev, struct fpd_driver_obj *driver_obj) {
	int ret;

	if ( dma_set_mask_and_coherent(&dev->dev, DMA_BIT_MASK(28)) ) {
		dev_warn(&dev->dev, "28-bit DMA addressing not available\n");
		ret = 1;
		goto err;
	}

	pci_set_master(dev);
	driver_obj->cpu_addr = dma_alloc_coherent(&dev->dev, fpd_dma_size, 
		&driver_obj->device_dma_addr, GFP_KERNEL);
	driver_obj->dma_enabled = true;

	dev_info(&dev->dev, "DMA Enabled!\n\tdevice_addr=0x%llx\n\tcpu_addr=%p\n\tphys_addr=0x%llx\n", 
		driver_obj->device_dma_addr, driver_obj->cpu_addr, virt_to_phys(driver_obj->cpu_addr));
	
	return 0;
err:
	return ret;
}

static irqreturn_t fpd_irq_handler(int irq, void *arg) {
	pr_info("Got irq from number %d\n", irq);
	return IRQ_HANDLED;
}

static int fpd_pci_probe(struct pci_dev *dev, const struct pci_device_id *id) {
	struct fpd_driver_obj *driver_obj;
	unsigned int i, j;
	int ret;

	driver_obj = kzalloc(sizeof(struct fpd_driver_obj), GFP_KERNEL);

	ret = pci_enable_device(dev);
	if (ret) {
		dev_err(&dev->dev, "pci_enable_device FAILED\n");
		goto err;
	}

#ifdef REQ_REGION
	ret = pci_request_regions(dev, KBUILD_MODNAME);
	if (ret) {
		dev_err(&dev->dev, "pci_request_regions FAILED\n");
		pci_disable_device(dev);
		goto err;
	}
#endif

#ifdef REQ_MSI
	ret = pci_alloc_irq_vectors(dev, FPD_NR_IRQS, FPD_NR_IRQS, PCI_IRQ_MSI | PCI_IRQ_MSIX);
	if (ret < 0) {
		dev_err(&dev->dev, "Unable to allocate irq vectors\n");
		goto err_free_region;
	}

	for (i = 0; i < FPD_NR_IRQS; ++ i) {
		driver_obj->irq_num[i] = pci_irq_vector(dev, i);
		if (driver_obj->irq_num[i] < 0) {
			dev_err(&dev->dev, "Unable to get Linux irq number\n");
			goto err_free_irq;
		}
		ret = request_irq(driver_obj->irq_num[i], fpd_irq_handler, IRQF_SHARED, "fpd_irq", &dev->dev);
		if (ret) {
			dev_err(&dev->dev, "Unable to request irq\n");
			goto err_free_irq;
		}
	}
#endif
	dev_set_drvdata(&dev->dev, driver_obj);
	fpd_setup_dma(dev, driver_obj);

	dev_info(&dev->dev, "init ok!\n");
	return 0;
err_free_irq:
	for (j = 0; j < i; ++ j) {
		free_irq(driver_obj->irq_num[j], &dev->dev);
	}
	pci_free_irq_vectors(dev);
err_free_region:
#ifdef REQ_REGION
	pci_release_regions(dev);
#endif
err:
	pci_disable_device(dev);
	kfree(driver_obj);
	return ret;
}

void fpd_pci_remove(struct pci_dev *dev) {
	struct fpd_driver_obj *driver_obj;
	unsigned int i;

	driver_obj = dev_get_drvdata(&dev->dev);
	if (driver_obj == NULL) return;
#ifdef REQ_REGION
	pci_release_regions(dev);
#endif
#ifdef REQ_MSI
	for (i = 0; i < FPD_NR_IRQS; ++ i) {
		free_irq(driver_obj->irq_num[i], &dev->dev);
	}
	pci_free_irq_vectors(dev);
#endif
	if (driver_obj->dma_enabled) {
		pci_clear_master(dev);
		dma_free_coherent(&dev->dev, fpd_dma_size, driver_obj->cpu_addr, driver_obj->device_dma_addr);
	}
	pci_disable_device(dev);
	kfree(driver_obj);
}

static struct pci_driver fpd_driver = {
	.name = KBUILD_MODNAME,
	.id_table = fpd_pci_ids,
	.probe = fpd_pci_probe,
	.remove = fpd_pci_remove
};

static int __init fpd_init(void) {
	return pci_register_driver(&fpd_driver);
}

static void __exit fpd_exit(void) {
	pci_unregister_driver(&fpd_driver);
}




module_init(fpd_init);
module_exit(fpd_exit);