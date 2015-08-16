#include "MeUsb.h"

#define p_dev_descr ((PUSB_DEV_DESCR)RECV_BUFFER)
#define p_cfg_descr ((PUSB_CFG_DESCR_LONG)RECV_BUFFER)

unsigned char endp_out_addr;
unsigned char endp_out_size;
unsigned char endp_in_addr;
unsigned char endp6_mode, endp7_mode;

unsigned char *cmd_buf;
unsigned char *ret_buf;
PUSB_ENDP_DESCR tmpEp;

#if defined(__AVR_ATmega32U4__)
  #define hardwareSerial Serial1
#else
  #define hardwareSerial Serial
#endif

//#define USB_DBG 1
MeUsb::MeUsb()
{
    useSoftSerial = false;
}
MeUsb::MeUsb(uint8_t s1, uint8_t s2)
{
  useSoftSerial = true;
  softSerial = new SoftwareSerial(s1,s2);
}

unsigned char MeUsb::USB_RD()
{
  delay(2); // stupid delay, the chip don't got any buffer
  if(useSoftSerial){
    if(softSerial->available()){
      unsigned char c = softSerial->read();
      return c&0xff;
    }
  }else{
      if(hardwareSerial.available()){
          unsigned char c = hardwareSerial.read();
          return c&0xff;
      }
  }
  return 0;
}

void MeUsb::USB_WR(unsigned char c)
{
  if(useSoftSerial){
    softSerial->write(c);
  }else{
    hardwareSerial.write(c);
  }
  delay(1);
#ifdef USB_DBG   
    Serial.print(">>");
    Serial.println(c,HEX);
#endif
}

int MeUsb::set_usb_mode(int mode)
{
  USB_WR(CMD_SET_USB_MODE);
  USB_WR(mode);
  endp6_mode=endp7_mode=0x80;
  return USB_RD();
}

unsigned char MeUsb::getIrq()
{
  USB_WR(CMD_GET_STATUS);
  delay(20);
  return USB_RD();
}

void MeUsb::toggle_send()
{
#ifdef USB_DBG   
    Serial.print("toggle send:");
    Serial.println(endp7_mode,HEX);
#endif
  USB_WR(CMD_SET_ENDP7);
  USB_WR( endp7_mode );
  endp7_mode^=0x40;
}

void MeUsb::toggle_recv()
{
  USB_WR( CMD_SET_ENDP6 );
  USB_WR( endp6_mode );
#ifdef USB_DBG   
    Serial.print("toggle recv:");
    Serial.println(endp6_mode,HEX);
#endif
  endp6_mode^=0x40;
}


unsigned char MeUsb::issue_token( unsigned char endp_and_pid )
{
  USB_WR( CMD_ISSUE_TOKEN );
  USB_WR( endp_and_pid );  /* Bit7~4 for EndPoint No, Bit3~0 for Token PID */
#ifdef USB_DBG
    Serial.print("issue token:");
    Serial.println(endp_and_pid,HEX);
#endif
  delay(2);
  return getIrq();
}

void MeUsb::wr_usb_data( unsigned char len, unsigned char *buf )
{
#ifdef USB_DBG   
    Serial.print("usb wr:");
    Serial.println(len);
#endif
  USB_WR( CMD_WR_USB_DATA7 );
  USB_WR( len );
  while( len-- ){
    USB_WR( *buf++ );
  }
}

unsigned char MeUsb::rd_usb_data( unsigned char *buf )
{
  unsigned char i, len;
  USB_WR( CMD_RD_USB_DATA );
  len=USB_RD();
#ifdef USB_DBG
    Serial.print("usb rd:");
    Serial.println(len);
#endif
  for ( i=0; i!=len; i++ ) *buf++=USB_RD();
  return( len );
}

int MeUsb::get_version(){
  USB_WR(CMD_GET_IC_VER);
  return USB_RD();
}

void MeUsb::set_freq(void)
{
  USB_WR(0x0b);
  USB_WR(0x17);
  USB_WR(0xd8);
}

unsigned char MeUsb::set_addr( unsigned char addr )
{
  unsigned char irq;
  USB_WR(CMD_SET_ADDRESS);
  USB_WR(addr);
  irq = getIrq();
  if(irq==USB_INT_SUCCESS){
    USB_WR(CMD_SET_USB_ADDR);
    USB_WR(addr);
  }
  return irq;
}

unsigned char MeUsb::set_config(unsigned char cfg){
  endp6_mode=endp7_mode=0x80; // reset the sync flags
  USB_WR(CMD_SET_CONFIG);
  USB_WR(cfg);
  return getIrq();
}

unsigned char MeUsb::clr_stall6(void)
{
  USB_WR( CMD_CLR_STALL );
  USB_WR( endp_out_addr | 0x80 );
  endp6_mode=0x80;
  return getIrq();
}

unsigned char MeUsb::get_desr(unsigned char type)
{
  USB_WR( CMD_GET_DESCR );
  USB_WR( type );  /* description type, only 1(device) or 2(config) */
  return getIrq();
}

unsigned char MeUsb::host_recv()
{
  unsigned char irq;
  toggle_recv();
  irq = issue_token( ( endp_in_addr << 4 ) | DEF_USB_PID_IN );
  if(irq==USB_INT_SUCCESS){
    int len = rd_usb_data(RECV_BUFFER);
#ifdef USB_DBG
    for(int i=0;i<len;i++){
      // point hid device
      Serial.print("0x");
      Serial.println((int)RECV_BUFFER[i],HEX);
    }
    Serial.println();
#endif
    stallCount = 0;
    return len;
  }else if(irq==USB_INT_DISCONNECT){
    device_online = false;
    device_ready = false;
#ifdef USB_DBG   
    Serial.println("##### disconn #####");
#endif
    return 0;
  }else{
    clr_stall6();
#ifdef USB_DBG   
    Serial.println("##### stall #####");
#endif
    delay(10);
    /*
    stallCount++;
    if(stallCount>10){
      device_online = false;
      device_ready = false;
      resetBus();
    }
    */
    return 0;
  }
}

void MeUsb::resetBus()
{
  int c;
  c = set_usb_mode(7);
#ifdef USB_DBG   
      Serial.print("set mode 7: ");
      Serial.println(c,HEX);
#endif
  delay(10);
  c = set_usb_mode(6);
#ifdef USB_DBG   
      Serial.print("set mode 6: ");
      Serial.println(c,HEX);
#endif
  delay(10);
}

void MeUsb::init(char type)
{
  usb_online = false;
  device_online = false;
  device_ready = false;
  usbtype = type;
  if(useSoftSerial){
    softSerial->begin(9600);
  }else{
    hardwareSerial.begin(9600);
  }
}

int MeUsb::initHIDDevice()
{
  int irq, len, address;
  if(usbtype==USB1_0) set_freq(); //work on a lower freq, necessary for usb
  irq = get_desr(1);
#ifdef USB_DBG   
      Serial.print("get des irq: ");
      Serial.println(irq,HEX);
#endif
  if(irq==USB_INT_SUCCESS){
      len = rd_usb_data( RECV_BUFFER );
#ifdef USB_DBG   
      Serial.print("descr1 len: ");
      Serial.print(len,HEX);
      Serial.print(" type: ");
      Serial.println(p_dev_descr->bDescriptorType,HEX);
#endif
      irq = set_addr(2);
      if(irq==USB_INT_SUCCESS){
        irq = get_desr(2); // max buf 64byte, todo:config descr overflow
        if(irq==USB_INT_SUCCESS){
          len = rd_usb_data( RECV_BUFFER );
//#ifdef USB_DBG   
//          Serial.printf("descr2 len %d class %x subclass %x\r\n",len,p_cfg_descr->itf_descr.bInterfaceClass, p_cfg_descr->itf_descr.bInterfaceSubClass); // interface class should be 0x03 for HID
//          Serial.printf("num of ep %d\r\n",p_cfg_descr->itf_descr.bNumEndpoints);
//          Serial.printf("ep0 %x %x\r\n",p_cfg_descr->endp_descr[0].bLength, p_cfg_descr->endp_descr[0].bDescriptorType);
//#endif
          if(p_cfg_descr->endp_descr[0].bDescriptorType==0x21){ // skip hid des
            tmpEp = (PUSB_ENDP_DESCR)((char*)(&(p_cfg_descr->endp_descr[0]))+p_cfg_descr->endp_descr[0].bLength); // get the real ep position
          }
//#ifdef USB_DBG   
//          Serial.printf("endpoint %x %x\r\n",tmpEp->bEndpointAddress,tmpEp->bDescriptorType);
//#endif
          endp_out_addr=endp_in_addr=0;
          address =tmpEp->bEndpointAddress;  /* Address of First EndPoint */
          // actually we only care about the input end points
          if( address&0x80 ){
            endp_in_addr = address&0x0f;  /* Address of IN EndPoint */
          }else{  /* OUT EndPoint */
            endp_out_addr = address&0x0f;
            endp_out_size = p_cfg_descr->endp_descr[0].wMaxPacketSize;  /* 
            Length of Package for Received Data EndPoint */
            if( endp_out_size == 0 || endp_out_size > 64 )
              endp_out_size = 64;
          }
          // todo: some joystick with more than 2 node
          // just assume every thing is fine, bring the device up
          irq = set_config(p_cfg_descr->cfg_descr.bConfigurationvalue);
          if(irq==USB_INT_SUCCESS){
            USB_WR( CMD_SET_RETRY );  // set the retry times
            USB_WR( 0x25 );
            USB_WR( 0x85 );
            device_ready = true;
            return 1;
          }
        }
        
      }
  }
  return 0;
}

int MeUsb::probeDevice()
{
  int c;
  if(!usb_online){
    USB_WR( CMD_CHECK_EXIST );
    USB_WR( 0x5A);
    c = USB_RD(); // should return 0xA5
    Serial.println(c);
    if(c!=0xA5) return 0;
    usb_online = true;
    resetBus();
  }
  
  c = getIrq();
  if(c!=USB_INT_CONNECT) return 0;
  resetBus(); // reset bus and wait the device online again
  c=0;
  while(c!=USB_INT_CONNECT){
    delay(500); // some device may need long time to get ready
    c = getIrq();
    Serial.print("waiting:");
    Serial.println(c,HEX);
  }
  if( initHIDDevice()==1)
    device_online=true;
}


