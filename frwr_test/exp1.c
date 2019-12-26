#include <linux/version.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/string.h>
#include <linux/parser.h>
#include <linux/proc_fs.h>
#include <linux/inet.h>
#include <linux/list.h>
#include <linux/in.h>
#include <linux/device.h>
#include <linux/pci.h>
#include <linux/time.h>
#include <linux/random.h>
#include <linux/signal.h>
#include <linux/proc_fs.h>

#include <asm/atomic.h>
#include <asm/pci.h>

#include <rdma/ib_verbs.h>
#include <rdma/rdma_cm.h>

struct rdma_cm_id *cm_id = NULL;	

static int exp1_event_handler(struct rdma_cm_id *cma_id,
				   struct rdma_cm_event *event)
{
	printk("%s:\n", __func__);
	return 0;
}

static void fill_sockaddr(struct sockaddr_storage *sin)
{
	memset(sin, 0, sizeof(*sin));
	if(1) /* AF_INET */ {
		struct sockaddr_in *sin4 = (struct sockaddr_in *)sin;
		sin4->sin_family = AF_INET;
		in4_pton("192.168.1.3", -1, (u8*)&sin4->sin_addr.s_addr, -1, NULL);
		sin4->sin_port = htons(0x4421);
	}
}

static int exp_create_qp(struct rdma_cm_id *id, 
		struct ib_pd *pd, struct ib_cq *cq)
{
	struct ib_qp_init_attr init_attr;

	memset(&init_attr, 0, sizeof(init_attr));
	init_attr.cap.max_send_wr = 64;
	init_attr.cap.max_recv_wr = 64;
	
	init_attr.cap.max_recv_sge = 1;
	init_attr.cap.max_send_sge = 1;
	init_attr.qp_type = IB_QPT_RC;
	init_attr.send_cq = cq;
	init_attr.recv_cq = cq;
	init_attr.sq_sig_type = IB_SIGNAL_REQ_WR;

	return rdma_create_qp(id, pd, &init_attr);
}

static void exp_cq_event_handler(struct ib_cq *cq, void *ctx)
{

}

static int __init exp_init(void)
{
	struct ib_mr *mr = NULL;
	struct ib_pd *pd;
	int ret;
	struct sockaddr_storage sin;
	struct ib_reg_wr fr;
	struct ib_send_wr  *bad;
	struct scatterlist sg = {0};
	struct ib_cq_init_attr attr = {0};
	struct ib_cq *cq;
	struct ib_wc wc;

	cm_id = rdma_create_id(&init_net, exp1_event_handler, NULL, RDMA_PS_TCP, IB_QPT_RC);
	if (IS_ERR(cm_id)) {
		ret = PTR_ERR(cm_id);
		printk( "rdma_create_id error %d\n", ret);
		goto out;
	}

	fill_sockaddr(&sin);

	ret = rdma_bind_addr(cm_id, (struct sockaddr *)&sin);
	if (ret) {
		printk("rdma_bind_addr error %d\n", ret);
		goto out2;
	}

	pd = ib_alloc_pd(cm_id->device, 0);
	if (IS_ERR(pd)) {
		printk("ib_alloc_pd failed\n");
		goto out2;
	}

	attr.cqe = 128;
	attr.comp_vector = 0;
	cq = ib_create_cq(cm_id->device, exp_cq_event_handler, NULL,
			      NULL, &attr);
	if (IS_ERR(cq)) {
		printk("create_cq failed \n");
		goto out3;
	}

	ret = exp_create_qp(cm_id, pd, cq);
	if (ret <0) {
		printk("create_qp failed \n");
		goto out4;
	}

	ret = ib_req_notify_cq(cq, IB_CQ_NEXT_COMP);
	if (ret) {
		printk("ib_req_notify_cq failed %d\n", ret);
		goto out5;
	}

	mr = ib_alloc_mr(pd, IB_MR_TYPE_MEM_REG, 64);
	if (IS_ERR(mr)) {
		ret = PTR_ERR(mr);
		printk(" reg_mr failed %d\n", ret);
		goto out5;
	}

	sg_dma_address(&sg) = (dma_addr_t)0xcafebabe0000ULL;
	sg_dma_len(&sg) = 8192;
	ret = ib_map_mr_sg(mr, &sg, 1, NULL, PAGE_SIZE);
	if (ret <= 0) {
		printk("ib_map_mr_sge err %d\n", ret);
		goto out6;
	}

	fr.wr.opcode = IB_WR_REG_MR;
	fr.access = IB_ACCESS_REMOTE_WRITE | IB_ACCESS_LOCAL_WRITE;
	fr.mr = mr;
	fr.wr.next = NULL;

	ret = ib_post_send(cm_id->qp, &fr.wr, &bad);
	if (ret) {
		printk("ib_post_send failed %d\n", ret);
		goto out6;	
	}

	ret = ib_poll_cq(cq, 1, &wc);
	if (ret < 0) {
		printk("ib_poll_cq failed %d\n", ret);
		goto out6;	
	}

	printk("mr-iova : %lx\n", mr->iova);
	return 0;
out6:
	ib_dereg_mr(mr);
out5:
	ib_destroy_qp(cm_id->qp);
out4:
	ib_destroy_cq(cq);
out3:
	ib_dealloc_pd(pd);
out2:
	rdma_destroy_id(cm_id);
	cm_id = NULL;
out:
	return 0;
}

static void __exit exp_cleanup(void)
{
	if (cm_id)
		rdma_destroy_id(cm_id);
}

module_init(exp_init);
module_exit(exp_cleanup);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("rdma experiments");
MODULE_AUTHOR("Syed S");
