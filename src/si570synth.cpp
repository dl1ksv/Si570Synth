#include "si570synth.h"
#include <QMessageBox>
#include <math.h>

//#define VENDOR_ID    0x16C0
//#define PRODUCT_ID   0x05DC
#define VENDOR_ID    0x04d8
#define PRODUCT_ID   0x000c

/* bmRequestType field in USB setup:
 * d t t r r r r r, where
 * d ..... direction: 0=host->device, 1=device->host
 * t ..... type: 0=standard, 1=class, 2=vendor, 3=reserved
 * r ..... recipient: 0=device, 1=interface, 2=endpoint, 3=other
 */
/**FA SY command table
 * Writes
 * command:  0x30 get register content and load to SI570 ( 6 bytes )
 * command:  0x32 get frequency and load to SI570        ( 4 bytes )
 * command:  0x33 write new crystal frequency to EEPROM  ( 4 bytes )
 *
 * Setup
 * bRequest:    1 set port directions
 * bRequest:    2 read ports
 * bRequest:    3 read port states
 * bRequest:    4 set ports
 * bRequest:    5 send I2C start sequence
 * bRequest:    6 send I2C stop sequence
 * bRequest:    7 send byte to I2C, reset error counter
 * bRequest:    8 send word to I2C, reset error counter
 * bRequest:    9 send dword to I2C, reset error counter
 * bRequest: 0x0a send word to I2C with start and stop sequence
 * bRequest: 0x0b receive word from I2C with start and stop sequence
 * bRequest: 0x0c modify I2C clock
 * bRequest: 0x0f Reset by Watchdog
 * bRequest: 0x10 EEPROM write byte value=address, index=data
 * bRequest: 0x11 EEPROM read byte "value"=address
 * /////SI570 specific code/////////////////////////////////////////////////////
 * bRequest: 0x20 SI570: write byte from register index
 * bRequest: 0x21 SI570: read byte to register index
 * bRequest: 0x22 SI570: freeze NCO
 * bRequest: 0x23 SI570: unfreeze NCO
 * bRequest >= 0x30
 * & bRequest <0x37 use usbFunctionWrite to transfer data
 * bRequest: 0x3f SI570: read out frequency control registers
 * bRequest: 0x40 return number of I2C transmission errors
 * bRequest: 0x41	set/reset init freq status
 * bRequest: 0x50 set RXTX and get cw-key status
 * bRequest: 0x51 read SDA and cw key level simultaneously
 *
 **/

#define RXTX 0x50;
#define READ_FREQUENCY_CONTROL 0x3f
#define READ_EEPROM_BYTE 0x11
#define WRITE_CRYSTAL_FREQ 0x33

#define F_CAL_STATUS 1  //1 byte
#define F_CRYST 2       //4 byte
static const double fcryst=114.285;
static const double fdcomin=4850;
static const double fdcomax=5670;


Si570Synth::Si570Synth()
{
  widget.setupUi(this);
  toogleRxTx=0;
  connect(widget.startTest,SIGNAL(clicked()),this,SLOT(getInfo()));
  connect(widget.rxTx,SIGNAL(clicked()),this,SLOT(switchRxTx()));
  connect(widget.readSI570,SIGNAL(clicked()),this,SLOT(readRegisters()));
  connect(widget.setFreq,SIGNAL(clicked()),this,SLOT(setFrequency()));
  connect(widget.startCalibrate,SIGNAL(clicked()),this,SLOT(calibrate()));
  if (libusb_init(NULL) < 0)
    QMessageBox::critical(this,"Error","Konnte die usb library nicht initialisieren");
  else libusb_set_debug (NULL,3);
  devh=NULL;
}

Si570Synth::~Si570Synth()
{
  if(devh !=0)
  {
    libusb_release_interface(devh,0);
    libusb_close(devh);
  }
  libusb_exit(NULL);
}
void Si570Synth::getInfo()
{
  int rc;
  unsigned char charstring[255];
  bool success;
  const char *c;
  int vendorid,productid;
  c= (char *) charstring;
  QString s;
  libusb_device *device;
  libusb_device_descriptor device_descriptor;
  libusb_config_descriptor *config_descriptor;
  const libusb_interface *interface;
  const libusb_interface_descriptor *interdesc;
  const libusb_endpoint_descriptor *epdesc;
  vendorid=0;
  vendorid=widget.vendorId->text().toInt(&success,16);
  if(!success | (vendorid == 0))
  {
    QMessageBox::critical(this,"Error","Ungültige vendorid");
    return;
  }
  productid=widget.productId->text().toInt(&success,16);
  if(!success | (productid == 0))
  {
    QMessageBox::critical(this,"Error","Ungültige productid");
    return;
  }
  devh=libusb_open_device_with_vid_pid (NULL, vendorid ,productid );
  if (devh == 0)
  {
    QMessageBox::critical(this,"Error","Konnte Device nicht finden");
    success=false;
    return;
  }
  else if (libusb_claim_interface(devh, 0) < 0 )
    {
    QMessageBox::critical(this,"Error","Konnte Interface nicht anmelden");
    success=false;
    return;
    }
  if(success)
    {
      widget.startTest->setEnabled(false);
      widget.readSI570->setEnabled(true);
      widget.rxTx->setEnabled(true);
      widget.setFreq->setEnabled(true);
      widget.startCalibrate->setEnabled(true);
    }
  device=libusb_get_device(devh);
  rc=libusb_get_device_descriptor(device,&device_descriptor);
  if(rc == 0)
  {
    widget.queryResult->append(QString("**** Device Informations ****\n"));
    if (device_descriptor.bcdUSB == 272 )
      widget.queryResult->append("USB Type: USB 1.1");
    else if(device_descriptor.bcdUSB == 512)
      widget.queryResult->append("USB Type: USB 2.0");
    s=QString("Device class: %1").arg(device_descriptor.bDeviceClass);
    widget.queryResult->append(s);
    if (device_descriptor.iManufacturer > 0)
    {
      rc = libusb_get_string_descriptor_ascii(devh,device_descriptor.iManufacturer,charstring,255);
      if (rc >0 )
      {
        s=QString("Manufacturer: ")+QString( c);
        widget.queryResult->append(s);
      }
    }
    if (device_descriptor.iProduct > 0)
    {
      rc = libusb_get_string_descriptor_ascii(devh,device_descriptor.iProduct,charstring,255);
      if (rc >0 )
      {
        s=QString("Product: ")+QString( c);
        widget.queryResult->append(s);
        widget.deviceName->setText(QString( c));
      }
    }
    if (device_descriptor.iSerialNumber > 0)
    {
      rc = libusb_get_string_descriptor_ascii(devh,device_descriptor.iSerialNumber,charstring,255);
      if (rc >0 )
      {
        s=QString("Serial number: ")+QString( c);
        widget.queryResult->append(s);
      }
    }
    int numconfigs=device_descriptor.bNumConfigurations;
    s=QString("\nNumber of possible Configurations: %1").arg(numconfigs);
    widget.queryResult->append(s);
    rc=libusb_get_config_descriptor(device,0,&config_descriptor);
    if(rc== 0)
    {
      for(int i=0; i< numconfigs;i++)
      {
        s=QString("\n**** Configuration %1 ****\n").arg(i);
        widget.queryResult->append(s);
        int numinterfaces=config_descriptor[i].bNumInterfaces;
        s=QString(" -- Interfaces: %1").arg(numinterfaces);
        widget.queryResult->append(s);
        for(int j=0; j < numinterfaces;j++)
        {
          interface = &config_descriptor->interface[j];
          s=QString("Number of alternate settings: %1").arg(interface->num_altsetting);
	        for(int k=0; k<interface->num_altsetting; k++)
          {
	            interdesc = &interface->altsetting[k];
              s=QString("++++ Interface number %1 ++++\n").arg(interdesc->bInterfaceNumber);
              widget.queryResult->append(s);
              s=QString("Number of endpoints: %1").arg(interdesc->bNumEndpoints);
              widget.queryResult->append(s);
	            for(int k=0; k<(int)interdesc->bNumEndpoints; k++)
              {
	                epdesc = &interdesc->endpoint[k];
                  s=QString("Descriptor Type: %1").arg(epdesc->bDescriptorType);
                  widget.queryResult->append(s);
                  s=QString("EP Address: %1").arg(epdesc->bEndpointAddress);
                  widget.queryResult->append(s);
	            }
          }
        }
        int p=(config_descriptor[i]).MaxPower*2;
        s=QString("\nMaximum power: %1 mA").arg(p);
        widget.queryResult->append(s);
      }
      libusb_free_config_descriptor(config_descriptor);
    }
  }
  else  qDebug("RC= %i",rc);
  if(success)
    getRxTxCwStatus();
}
void Si570Synth::switchRxTx()
{
  int rc;
  unsigned char buffer[3];
  for(int i=0; i<3;i++)
    buffer[i]=0;
  unsigned char bmRequestType=LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE |LIBUSB_RECIPIENT_ENDPOINT|LIBUSB_ENDPOINT_IN ;
  rc=libusb_control_transfer(devh,bmRequestType,0x50,toogleRxTx,0,buffer,3,5000);
  if (rc > 0)
   widget.rxtxResult->setText(QString("0x%1").arg(buffer[0],4,16));
  getRxTxCwStatus();

  toogleRxTx++;
  toogleRxTx %=2;
  
}
void Si570Synth::readRegisters()
{
   int rc,i;
   unsigned int HS_DIV,N1;
   int hs_divTab[]={4,5,6,7,-1,9,-1,11};
   long int RFREQ;
  unsigned char buffer[6];
  for(i=0; i<6 ;i++)
    buffer[i]=0;
  unsigned char bmRequestType=LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE |LIBUSB_RECIPIENT_ENDPOINT|LIBUSB_ENDPOINT_IN ;
  rc=libusb_control_transfer(devh,bmRequestType,READ_FREQUENCY_CONTROL,0x55,0,buffer,6,5000);
  if(rc > 0)
  {
    widget.queryResult->clear();
    widget.queryResult->append("Register Content");
    for(i=0;i< 6;i++)
      widget.queryResult->append(QString("   %1          %2 Hex").arg(i+7,2,10,QLatin1Char('0')).arg(buffer[i],2,16,QLatin1Char('0')));
    widget.queryResult->append("-----------------------------");
    HS_DIV=buffer[0]>>5;
    widget.queryResult->append(QString("HS_DIV: %1").arg(HS_DIV));
    N1=(buffer[0] & 0x1F)*4+ (buffer[1]>>6);
    widget.queryResult->append(QString("N1:     %1").arg(N1));
    N1++;
    RFREQ=(buffer[1] & 0x1F);
    RFREQ <<=8;
    RFREQ=RFREQ | buffer[2];
    RFREQ <<=8;
    RFREQ=RFREQ | buffer[3];
    RFREQ <<=8;
    RFREQ=RFREQ | buffer[4];
    RFREQ <<=8;
    RFREQ=RFREQ | buffer[5];
    widget.queryResult->append(QString("RFREQ: %1 0x%2").arg(RFREQ).arg(RFREQ,0,16));
 // Now we read the calibration status
    actCrystFreq=fcryst;
    rc=libusb_control_transfer(devh,bmRequestType,READ_EEPROM_BYTE,F_CAL_STATUS,0,buffer,6,5000);
    if(rc > 0)
    {
      if(buffer[0] ==0xff)
        widget.queryResult->append("Device not calibrated");
      else
      {
        for(i=0;i<4;i++)
          rc=libusb_control_transfer(devh,bmRequestType,READ_EEPROM_BYTE,F_CRYST+i,0,&buffer[i],1,5000);
        double fout,f;
         widget.queryResult->append(QString("CalibrationFreq (hex) 0x%1 0x%2 0x%3 0x%4").arg(buffer[0],0,16).arg(buffer[1],0,16).arg(buffer[2],0,16).arg(buffer[3],0,16));

        fout=  buffer[0];
        f=buffer[1];
        fout +=f/256.;
        f=buffer[2];
        fout +=f/(256.*256.);
        f=buffer[3];
        fout+=f/(256.*256.*256.);
        actCrystFreq=fout;
        widget.queryResult->append(QString("Calibrated XTAL Frequency: %1").arg(fout));
      }
    }
    double freq=((double) RFREQ)*actCrystFreq/pow(2,28);
    widget.queryResult->append(QString("fdco: %1 Mhz").arg(freq));
    HS_DIV=hs_divTab[HS_DIV];
    freq=freq/(HS_DIV*N1);
    widget.queryResult->append(QString("Frequency: %1").arg(freq));
    widget.displayFreq->setText(QString("%1").arg(freq,9,'f',6));
  }
}

void Si570Synth::setFrequency()
{
  double freq,divider;
  bool ok;
  double fdco;
  int N1,hs_div,i,hs_divIndex;
  unsigned char buffer[6];
  long int RFREQ;
  int hs_divTab[]={4,5,6,7,-1,9,-1,11};
  freq=widget.displayFreq->text().toDouble(&ok);
  if(!ok)
  {
    QMessageBox::critical(this,"Error","Ungültige Frequenz");
    return;
  }
  divider=(fdcomin+fdcomax)/(2.*freq);
  N1=-1;
  fdco=1E100;
  for(i=7;i>=0;i--) // Find maximal hs_div and minimal N1
    if(hs_divTab[i]>0)
  {
      int n1;
      double f;
      hs_div=hs_divTab[i];
      //first, check minimal value n1 = 1
      n1=1;
      f=freq*n1*hs_div;
      if(( f>=fdcomin)&& (f <=fdcomax))
      {
        if(f < fdco)
        {
         fdco=f;
         N1=n1;
         hs_divIndex=i;
        }
       break;
      }

      n1=divider/hs_div;
      if(n1 >128)
        n1=128;
      else
        n1 =(n1/2)*2;
      f=freq*n1*hs_div;
      if(( f>=fdcomin)&& (f <=fdcomax))
      {
        if(f < fdco)
        {
         fdco=f;
         N1=n1;
         hs_divIndex=i;
        }
       break;
      }
      if ( n1 < 128)
        {
          n1 +=2;
          f=freq*n1*hs_div;
          if(( f>=fdcomin)&& (f <=fdcomax))
          {
            if(f < fdco)
            {
             fdco=f;
             N1=n1;
             hs_divIndex=i;
            }
           break;
          }
        }
  }
  if (N1 <0)
  {
    widget.queryResult->append("No vaild combination found");
    return;
  }
  RFREQ=(fdco/actCrystFreq)*pow(2,28);
  widget.queryResult->append(QString("Found N1: %1 ,HS_DIV: %2 , RFREQ: %3 ,fdco: %4").arg(N1).arg(hs_divIndex).arg(RFREQ).arg(fdco));
  widget.queryResult->append(QString("RFREQ 0x%1").arg(RFREQ,0,16));
  // Now convert to register values
  for(i=5;i >= 1; i--)
  {
    buffer[i]=RFREQ & 0xFF;
    RFREQ= RFREQ >>8;
  }
  N1--;
  buffer[1]=(buffer[1] & 0x3F) | (((N1 & 0x03) << 6 ) & 0xc0);
  buffer[0]=((N1 >> 2 ) & 0x1F ) | (hs_divIndex << 5);
  for(i=0;i < 6;i++)
//    qDebug("Register[%d]: %x",i+7,buffer[i]);
   widget.queryResult->append(QString("   %1          %2 Hex").arg(i+7,2,10,QLatin1Char('0')).arg(buffer[i],2,16,QLatin1Char('0')));
    unsigned char bmRequestType=LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE |LIBUSB_RECIPIENT_ENDPOINT|LIBUSB_ENDPOINT_OUT ;
  i=libusb_control_transfer(devh,bmRequestType,0x30,0x0755,0,buffer,6,5000);
  qDebug("Result of Operation: %d",i);
}

/**
void Si570Synth::readOscCalValue()
{
  int rc;
  unsigned char buffer[10];
  unsigned char bmRequestType=LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE |LIBUSB_RECIPIENT_ENDPOINT|LIBUSB_ENDPOINT_IN ;
  for (int i=0; i< 10;i++)
  {
    buffer[i]=0;
    rc=libusb_control_transfer(devh,bmRequestType,0x11,i,0,&buffer[i],1,5000);
    qDebug("rc: %i, value %x",rc,buffer[i]);
  }
}
 **/
void Si570Synth::calibrate()
{
 const double fcryst=114.285;
 double freq,calibrationfreq;
 unsigned char buffer[4];
 unsigned int *fint;
 bool ok;
 int rc;
 unsigned char bmRequestType=LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE |LIBUSB_RECIPIENT_ENDPOINT|LIBUSB_ENDPOINT_OUT ;
 freq=widget.displayFreq->text().toDouble(&ok);
 if(!ok)
 {
    QMessageBox::critical(this,"Error","Ungültige Ausgangsfrequenz");
    return;
 }
 calibrationfreq=widget.calibrationFrequency->text().toDouble(&ok);
 if(!ok)
 {
    QMessageBox::critical(this,"Error","Ungültige Calibrationfrequenz");
    return;
 }
 freq=calibrationfreq/freq*fcryst;
 if((freq < 110.) || (freq > 120.))
 {
    QMessageBox::critical(this,"Error","Ungültiges Calibrationsergebnis");
    return;
 }
 widget.queryResult->append(QString("Berechnete Quarzfrequenz: %1").arg(freq));

 rc=(int) freq;
 buffer[0]=rc;
 rc=(freq-rc)*pow(2,24);
 buffer[3]=(rc & 0xff);
 buffer[2]=(rc & 0xff00) >>8;
 buffer[1]=(rc & 0xff0000) >>16;
 fint = (unsigned int *) buffer;

 qDebug("fint: %d",*fint);
 widget.queryResult->append(QString("CalibrationFreq (hex) 0x%1 0x%2 0x%3 0x%4").arg(buffer[0],0,16).arg(buffer[1],0,16).arg(buffer[2],0,16).arg(buffer[3],0,16));
 rc=libusb_control_transfer(devh,bmRequestType,WRITE_CRYSTAL_FREQ,0,0,buffer,4,5000);
 qDebug("Result of calibration: %d",rc);
}
void Si570Synth::getRxTxCwStatus()
{
  int rc;
  unsigned char buffer[3];
  for(int i=0; i<3;i++)
    buffer[i]=0;
  unsigned char bmRequestType=LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE |LIBUSB_RECIPIENT_ENDPOINT|LIBUSB_ENDPOINT_IN ;
  rc=libusb_control_transfer(devh,bmRequestType,0x03,0,0,buffer,3,5000);
  if (rc > 0)
  {
    if(buffer[0] & 0x10)
      widget.rxtxStatus->setPixmap(QPixmap(QString::fromUtf8(":/green.png")));
    else
      widget.rxtxStatus->setPixmap(QPixmap(QString::fromUtf8(":/red.png")));
   }
  buffer[0]=0;
  rc=libusb_control_transfer(devh,bmRequestType,0x02,0,0,buffer,3,5000);
  if( rc > 0 )
  {
    if(buffer[0] & 0x02)
      widget.cwStatus->setPixmap(QPixmap(QString::fromUtf8(":/green.png")));
    else
      widget.cwStatus->setPixmap(QPixmap(QString::fromUtf8(":/red.png")));
  }
}
