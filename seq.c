/* seq.c
 * 
 *  used to send and receive bytes one by one to an usb device
 *  
 **/ 

#include <stdio.h>
#include <string.h>
#include <usb.h>

/* flag and macros for debugging */
int dodebug=1;
#define DEBUG(x) {if (dodebug) {fprintf(stderr,">");fprintf(stderr,x);fprintf(stderr,"n");}}
#define DEBUG2(x1,x2)  {if (dodebug) {fprintf(stderr,">");fprintf(stderr,x1,x2);fprintf(stderr,"n");}}
#define DEBUG4(x1,x2,x3,x4)  {if (dodebug) {fprintf(stderr,">");fprintf(stderr,x1,x2,x3,x4);fprintf(stderr,"n");}}
#define DEBUG5(x1,x2,x3,x4,x5)  {if (dodebug) {fprintf(stderr,">");fprintf(stderr,x1,x2,x3,x4,x5);fprintf(stderr,"n");}}



/* retrieve a handle to a specific usb device */
struct usb_device * findNbglinUsb(int vendor,int product){
  struct usb_device * searchDev;
  struct usb_bus *bus;
  usb_init();
  usb_find_busses();
  usb_find_devices();


    /* on all busses look for the specified usb device */
  for (bus = usb_busses; bus   ; bus = bus->next) {
    
    for (searchDev = bus->devices; searchDev ; searchDev = searchDev->next) {
      DEBUG5("found usb %s/%s %04X/%04X", bus->dirname, searchDev->filename, 
      searchDev->descriptor.idVendor, searchDev->descriptor.idProduct); 

      /* check if this device matches vendor and product  */
      if ( (searchDev->descriptor.idVendor == vendor ) &&
           (searchDev->descriptor.idProduct == product ) ) {
         DEBUG("found NbglinUsb");
        return searchDev;
      }
    } 
  }

}

/* initialiase a usb device */
usb_dev_handle * initNbglinUsb(struct usb_device *dev){
  usb_dev_handle *udev; 
  int c,i,ret;
  int config;
  udev = usb_open(dev);
  if (udev == NULL) {
    DEBUG("NbglinUsb: unable to open device");
    exit(-1);
  } 


  /* detach the usb device */
  for(c = 0; c < dev->descriptor.bNumConfigurations; c++) {
    config=c;
    #ifndef _WINBASE_H
    for(i = 0; i < dev->config[c].bNumInterfaces; i++) {
      usb_detach_kernel_driver_np(udev, i);
      DEBUG2("usb_detach_kernel_driver_np %i",i);
    }
    #endif
  }


  /* claim the interface */
  ret = usb_set_configuration(udev, dev->config[config].bConfigurationValue);
  DEBUG2("set conf=%d",ret);
  ret = usb_claim_interface(udev,0);
  DEBUG2("claim_stat=%d",ret);

  ret = usb_set_altinterface(udev,dev->config[config].interface[0].altsetting->bAlternateSetting);
  DEBUG2("alt_stat=%d",ret);
  DEBUG("NbglinUsb: opening device succeeded");
  return udev;

}

int closeNbglinUsb(usb_dev_handle * udev){
#ifdef _WINBASE_H
  /* could not find a cleaner way of releasing the connection on windows.
     if we don't do that, further connections are ignored by the device */
  usb_reset(udev);
#endif
  usb_release_interface(udev,0);
  usb_close(udev);
}

/*
 * process a command. All numbers X are in hexadecimal. A command is either:
 * rX: read X bytes, and print them on the stdout. 
 * X: write the byte X
 */
void process_command(char *arg,usb_dev_handle *udev){
  int command;
  char data[1];
  int i,ret;
  DEBUG2("command: %s",arg);

  


  if (arg[0]=='r') {
    unsigned int max=1;
    sscanf(arg+1,"%x",&max);
    /* read */
    for (i=0;i<max;i++){
      ret =usb_bulk_read(udev, 1, data, 1, 100);
      DEBUG4("%i: %d bytes read %hx ", i,ret,data[0]);
      if (ret==1) 
        fprintf(stdout,"%xn",data[0]);
      fflush(stdout);
      if (ret<0) {
        DEBUG("timeout");
    break;
     }
    }
  } else {
    /* write */
    sscanf(arg,"%x",&command);
     
    data[0]=command;
    ret =usb_bulk_write(udev, 1, data, 1, 100);
    DEBUG2("%d bytes written", ret);
  
  }
}


/* entry point */
int main(int argc,char ** args) 
{ 
  struct usb_device *dev;
  usb_dev_handle *udev; 
  int i;
  int startcommands=2;
  int vendor,product;

  /* check that there are at least two arguments */
  if (argc<3){
    printf(
      "Send and read bytes to the usb device. Syntax: %s vendor:product commands...n"
      "Commands available:n q for quiet.n r (or rX) read 1 (or X) unique bytes and prints them.n"
      " X write the byte to the usb.nAll numbers are in hexadecimaln"
      "E.g:  1234:1 1 2 3a ra 3b. connect to 0x1234:01 send 1 2 3a, read 10 bytes, send 3b",args[0]);
    exit(1);
  }
  
  /* first argument is the usb vendor and product ids */
  sscanf(args[1],"%x:%x",&vendor,&product);

  /* if the first command is q, then debug messages are not printed */
  if (args[2][0]=='q'){ 
    dodebug=0;
    startcommands=3;
  }
  
 
  /* find the usb device */
  dev=findNbglinUsb(vendor,product);
  if (!dev) {
    DEBUG("NbglinUsb not found");
    exit(-1);
  }
  
  /* initialise the usb device */
  udev=initNbglinUsb(dev);
  

  /* process each command */
  for(i=startcommands;i<argc;i++)
    process_command(args[i],udev);

  
  /* close the usb device */
  closeNbglinUsb(udev);
}
