#ifdef ENABLE_GUI
#ifdef ENABLE_UEFI
#include<stdlib.h>
#include<stdbool.h>
#include<string.h>
#include<Library/UefiBootServicesTableLib.h>
#include<Protocol/GraphicsOutput.h>
#include"version.h"
#include"logger.h"
#include"lvgl.h"
#include"gui.h"
#include"guidrv.h"
#define TAG "uefigop"

static int ww=800,hh=600;
static EFI_GRAPHICS_OUTPUT_PROTOCOL*gop;

static void uefigop_flush(lv_disp_drv_t*disp_drv,const lv_area_t*area,lv_color_t*color_p){
	gop->Blt(
		gop,
		(EFI_GRAPHICS_OUTPUT_BLT_PIXEL*)color_p,
		EfiBltBufferToVideo,
		0,0,
		area->x1,area->y1,
		area->x2-area->x1+1,
		area->y2-area->y1+1,
		0
	);
	lv_disp_flush_ready(disp_drv);
}

int uefigop_register(){
	if(EFI_ERROR(gBS->LocateProtocol(
		&gEfiGraphicsOutputProtocolGuid,
		NULL,
		(VOID**)&gop
	)))return -1;
	size_t s=ww*hh;
	static lv_color_t*buf=NULL;
	static lv_disp_buf_t disp_buf;
	if(!(buf=malloc(s*sizeof(lv_color_t)))){
		telog_error("malloc display buffer");
		return -1;
	}
	memset(buf,0,s);
	lv_disp_buf_init(&disp_buf,buf,NULL,s);
	lv_disp_drv_t disp_drv;
	lv_disp_drv_init(&disp_drv);
	disp_drv.buffer=&disp_buf;
	disp_drv.flush_cb=uefigop_flush;
	disp_drv.hor_res=ww;
	disp_drv.ver_res=hh;
	lv_disp_drv_register(&disp_drv);
	return 0;
}
static void uefigop_get_sizes(uint32_t*width,uint32_t*height){
	if(width)*width=ww;
	if(height)*height=hh;
}
static void uefigop_get_dpi(int*dpi){
	if(dpi)*dpi=200;
}
static bool uefigop_can_sleep(){
	return false;
}
struct gui_driver guidrv_uefigop={
	.name="uefigop",
	.drv_register=uefigop_register,
	.drv_getsize=uefigop_get_sizes,
	.drv_getdpi=uefigop_get_dpi,
	.drv_cansleep=uefigop_can_sleep
};
#endif
#endif
