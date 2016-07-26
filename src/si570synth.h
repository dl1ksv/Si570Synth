#ifndef _SI570SYNTH_H
#define	_SI570SYNTH_H

#include "ui_si570synth.h"
#include <libusb-1.0/libusb.h>


class Si570Synth : public QMainWindow {
    Q_OBJECT
public:
    Si570Synth();
    virtual ~Si570Synth();
private:
    Ui::Si570Synth widget;
    libusb_device_handle *devh;
    int toogleRxTx;
    double actCrystFreq; // actual ( calibrated Crystal frequency )
//    const double fcryst;
private slots:
    void getInfo();
    void switchRxTx();
    void readRegisters();
    void setFrequency();
    void calibrate();
    void getRxTxCwStatus();
};

#endif	/* _SI570SYNTH_H */
