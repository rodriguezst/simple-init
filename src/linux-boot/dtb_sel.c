/*
 *
 * Copyright (C) 2021 BigfootACA <bigfoot@classfun.cn>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 */

#include<Uefi.h>
#include<Library/UefiLib.h>
#include<Library/BaseLib.h>
#include<Library/BaseMemoryLib.h>
#include<Library/DevicePathLib.h>
#include<Library/MemoryAllocationLib.h>
#include<comp_libfdt.h>
#include<stdint.h>
#include"list.h"
#include"qcom.h"
#include"logger.h"
#include"fdtparser.h"
#include"internal.h"
#define TAG "fdt"

static list*fdts=NULL;

typedef struct fdt_info{
	size_t id;
	fdt address;
	size_t offset;
	size_t size;
	size_t mem_size;
	size_t mem_pages;
	char model[256];
	qcom_chip_info info;
	int64_t vote;
}fdt_info;

static void search_dtb(linux_boot*lb,void*buff,void**pos){
	char*model;
	fdt_info fi;
	int len;
	if(CompareMem(&fdt_magic,(*pos),4)!=0)goto fail;

	ZeroMem(&fi,sizeof(fdt_info));
	fi.size=fdt_totalsize((*pos));
	if(fi.size<=0||fi.size>=MAX_DTB_SIZE)goto fail;
	ZeroMem(buff,MAX_DTB_SIZE);
	CopyMem(buff,(*pos),fi.size);
	fi.offset=(*pos)-lb->dtb.address;
	(*pos)+=fi.size;
	if(fdt_check_header(buff)!=0)return;
	if(fdt_path_offset(buff,"/")!=0)return;
	fi.mem_pages=EFI_SIZE_TO_PAGES(ALIGN_VALUE(fi.size,MEM_ALIGN));
	fi.mem_size=EFI_PAGES_TO_SIZE(fi.mem_pages);
	if(!(fi.address=AllocateAlignedPages(fi.mem_pages,MEM_ALIGN)))return;
	ZeroMem(fi.address,fi.mem_size);
	CopyMem(fi.address,buff,fi.size);
	fi.id=MAX(list_count(fdts),0);

	model=(char*)fdt_getprop(fi.address,0,"model",&len);
	if(!model)model="Linux Device Tree Blob";
	AsciiStrCpyS(fi.model,sizeof(fi.model),model);
	qcom_parse_id(fi.address,&fi.info);
	if(fi.info.soc_id!=0)lb->status.qualcomm=true;

	list_obj_add_new_dup(&fdts,&fi,sizeof(fdt_info));
	return;
	fail:(*pos)++;
}

static int free_fdt(void*d){
	fdt_info*f=(fdt_info*)d;
	if(!f)return 0;
	if(f->address)FreePages(f->address,f->mem_pages);
	free(f);
	return 0;
}

static bool sort_fdt(list*f1,list*f2){
	LIST_DATA_DECLARE(d1,f1,fdt_info*);
	LIST_DATA_DECLARE(d2,f2,fdt_info*);
	return d1->vote<d2->vote;
}

static int check_dtbs(linux_boot*lb){
	list*f;
	qcom_chip_info chip_info;
	if(qcom_get_chip_info(lb,&chip_info)!=0)return -1;
	if((f=list_first(fdts)))do{
		LIST_DATA_DECLARE(fdt,f,fdt_info*);
		fdt->vote=qcom_check_dtb(&fdt->info,&chip_info);
		tlog_verbose(
			"dtb id %zu offset %zu size %zu vote %lld (%s)",
			fdt->id,fdt->offset,fdt->size,
			(long long)fdt->vote,
			fdt->model
		);
	}while((f=f->next));
	list_sort(fdts,sort_fdt);
	return 0;
}

static int select_dtb(linux_boot*lb,fdt_info*info){
	lb->status.dtb_id=(int64_t)info->id;
	tlog_info("select dtb id %zu (%s)",info->id,info->model);
	linux_boot_set_fdt(lb,info->address,info->size);
	return 0;
}

int linux_boot_select_fdt(linux_boot*lb){
	list*f=NULL;
	int r,ret=-1;
	fdt_info*fdt=NULL;
	void*buff,*end,*pos;
	if(!lb||!lb->dtb.address)return 0;
	list_free_all(fdts,free_fdt);
	lb->status.qualcomm=false,fdts=NULL;
	r=fdt_check_header(lb->dtb.address);
	if(r!=0){
		tlog_warn("invalid dtb head: %s",fdt_strerror(r));
		linux_file_clean(&lb->dtb);
		return -1;
	}
	if(fdt_totalsize(lb->dtb.address)==lb->dtb.size)return 0;
	if(!(buff=AllocateZeroPool(MAX_DTB_SIZE)))
		EDONE(tlog_error("allocate for fdt buff failed"));

	pos=lb->dtb.address,end=pos+lb->dtb.size;
	while(pos<end-4)search_dtb(lb,buff,&pos);
	FreePool(buff);
	tlog_info("found %d dtbs",fdts?list_count(fdts):0);
	if(!fdts)EDONE(tlog_warn("no dtb found"));
	if(lb->config&&lb->config->dtb_id>=0){
		if((f=list_first(fdts)))do{
			LIST_DATA_DECLARE(i,f,fdt_info*);
			if(i&&i->id==lb->config->dtb_id)fdt=i;
		}while((f=f->next));
		if(fdt)tlog_verbose("use pre selected dtb from config");
		else tlog_warn("specified dtb id not found");
	}else{
		if(lb->status.qualcomm){
			tlog_debug("detected qualcomm dtb");
			check_dtbs(lb);
		}
		if(
			!(f=list_first(fdts))||
			!(fdt=LIST_DATA(f,fdt_info*))
		)EDONE(tlog_warn("no dtb found"));
	}
	select_dtb(lb,fdt);
	ret=0;
	done:
	list_free_all(fdts,free_fdt);
	fdts=NULL;
	return ret;
}
