/**
 * @file     usb_endpoint.h
 * @brief    Universal Serial Bus Endpoint
 *
 * @version  V4.12.1.150
 * @date     13 Nov 2016
 */
#ifndef PROJECT_HEADERS_USB_ENDPOINT_H_
#define PROJECT_HEADERS_USB_ENDPOINT_H_
/*
 * *****************************
 * *** DO NOT EDIT THIS FILE ***
 * *****************************
 *
 * This file is generated automatically.
 * Any manual changes will be lost.
 */
#include "usb_defs.h"
#include "derivative.h"

namespace USBDM {

/** BDTs organised by endpoint, odd/even, transmit/receive */
extern EndpointBdtEntry endPointBdts[];

/** BDTs as simple array */
constexpr BdtEntry *bdts = (BdtEntry *)endPointBdts;

/** Endpoint state values */
enum EndpointState {
   EPIdle = 0,  //!< Idle
   EPDataIn,    //!< Doing a sequence of IN packets
   EPDataOut,   //!< Doing a sequence of OUT packets
   EPStatusIn,  //!< Doing an IN packet as a status handshake
   EPStatusOut, //!< Doing an OUT packet as a status handshake
   EPThrottle,  //!< Doing OUT packets but no buffers available (NAKed)
   EPStall,     //!< Endpoint is stalled
   EPComplete,  //!< Used for command protocol - new command available
};

/**
 * Class for generic endpoint
 */
class Endpoint {

protected:
   /** Data 0/1 Transmit flag */
   volatile Data0_1 txData1;

   /** Data 0/1 Receive flag */
   volatile Data0_1 rxData1;

   /** Odd/Even Transmit buffer flag */
   volatile EvenOdd txOdd;

   /** Odd/Even Receive buffer flag */
   volatile EvenOdd rxOdd;

   /** End-point state */
   volatile EndpointState state;

public:
   /** End point number */
   const int fEndpointNumber;

   /**
    * Constructor
    *
    * @param endpointNumber End-point number
    */
   constexpr Endpoint(int endpointNumber):
      txData1(DATA0),
      rxData1(DATA0),
      txOdd(EVEN),
      rxOdd(EVEN),
      state(EPIdle),
      fEndpointNumber(endpointNumber) {
   }

   virtual ~Endpoint() {}

   /**
    * Flip active odd/even buffer state
    *
    * @param usbStat Value from USB->STAT
    */
    void flipOddEven(uint8_t usbStat) {

      // Direction of transfer 0=>OUT, (!=0)=>IN
      bool isTx  = usbStat&USB_STAT_TX_MASK;

      // Odd/even buffer
      bool isOdd = usbStat&USB_STAT_ODD_MASK;

      if (isTx) {
         // Flip Transmit buffer
         txOdd = !isOdd;
      }
      else {
         // Flip Receive buffer
         rxOdd = !isOdd;
      }
   }

   /**
    * Stall endpoint
    */
   virtual void stall() = 0;

   /*
    * Clear Stall on endpoint
    */
   virtual void clearStall() = 0;
};

/**
 * Class for generic endpoint
 *
 * @tparam Info         Class describing associated USB hardware
 * @tparam ENDPOINT_NUM Endpoint number
 * @tparam EP_MAXSIZE   Maximum size of packet
 */
template<class Info, int ENDPOINT_NUM, int EP_MAXSIZE>
class Endpoint_T : public Endpoint {

public:
   /** Size of end-point buffer */
   static constexpr int BUFFER_SIZE  = EP_MAXSIZE;

protected:
   /** Pointer to hardware */
   static constexpr USB_Type volatile *usb = Info::usb;

   /** Buffer for Transmit & Receive data */
   uint8_t fDataBuffer[EP_MAXSIZE];

   /** Callback used on completion of transaction
    *
    * @param endpointState State of endpoint before completion \n
    * e.g. EPDataOut, EPStatusIn, EPLastIn, EPStatusOut\n
    * The endpoint is in EPIdle state now.
    */
   void (*fCallback)(EndpointState endpointState);

   /** Pointer to external data buffer for transmit/receive */
   volatile uint8_t* fDataPtr;

   /** Count of remaining bytes in external data buffer to transmit/receive */
   volatile uint16_t fDataRemaining;

   /** Count of data bytes transferred to/from data buffer */
   volatile uint16_t fDataTransferred;

   /**
    *  Indicates that the IN transaction needs to be
    *  terminated with ZLP if size is a multiple of EP_MAXSIZE
    */
   volatile bool fNeedZLP;

public:
   /**
    * Constructor
    */
   Endpoint_T() : Endpoint(ENDPOINT_NUM) {
      initialise();
   }

   /**
    * Gets pointer to USB data buffer
    *
    * @return Pointer to buffer
    */
    uint8_t *getBuffer() {
      return fDataBuffer;
   }
   /**
    * Gets size of last completed transfer
    *
    * @return Size of transfer
    */
    uint16_t getDataTransferredSize() {
      return fDataTransferred;
   }
   /**
    * Return end-point state
    *
    * @return Endpoint state
    */
    EndpointState getState() {
      return state;
   }

   /**
    * Set callback to execute at end of transaction
    *
    * @param callback The call-back function to execute
    */
    void setCallback(void (*callback)(EndpointState)) {
      fCallback = callback;
   }

   /**
    * Do callback if set
    *
    * @param state State of Endpoint
    */
    void doCallback(EndpointState state) {
      if (fCallback != nullptr) {
         fCallback(state);
      }
   }

   /**
    *  Indicates that the next IN transaction needs to be
    *  terminated with ZLP if modulo endpoint size
    *
    *  @param needZLP True to indicate need for ZLPs.
    *
    *  @note This flag is cleared on each transaction
    */
   void setNeedZLP(bool needZLP=true) {
      fNeedZLP = needZLP;
   }

   /**
    * Stall endpoint
    */
   virtual void stall() override {
      state = EPStall;
      usb->ENDPOINT[ENDPOINT_NUM].ENDPT |= USB_ENDPT_EPSTALL_MASK;
   }

   /**
    * Clear Stall on endpoint
    */
   virtual void clearStall() override {
      usb->ENDPOINT[ENDPOINT_NUM].ENDPT &= ~USB_ENDPT_EPSTALL_MASK;
      state               = EPIdle;
      txData1             = DATA0;
      rxData1             = DATA0;
   }

   /**
    * Initialise endpoint
    *  - Internal state
    *  - BDTs
    */
    void initialise() {
      txData1  = DATA0;
      rxData1  = DATA0;
      txOdd    = EVEN;
      rxOdd    = EVEN;
      state    = EPIdle;

      fDataPtr          = nullptr;
      fDataTransferred  = 0;
      fDataRemaining    = 0;
      fNeedZLP          = false;
      fCallback         = nullptr;

      // Assumes single buffer
      endPointBdts[ENDPOINT_NUM].rxEven.addr = nativeToLe32((uint32_t)fDataBuffer);
      endPointBdts[ENDPOINT_NUM].rxOdd.addr  = nativeToLe32((uint32_t)fDataBuffer);
      endPointBdts[ENDPOINT_NUM].txEven.addr = nativeToLe32((uint32_t)fDataBuffer);
      endPointBdts[ENDPOINT_NUM].txOdd.addr  = nativeToLe32((uint32_t)fDataBuffer);
   }

   /**
    * Start IN transaction [Transmit, device -> host, DATA0/1]
    *
    * @param state   State to adopt for transaction e.g. EPDataIn, EPStatusIn
    * @param bufSize Size of buffer to send (may be zero)
    * @param bufPtr  Pointer to buffer (may be NULL to indicate fDatabuffer is being used directly)
    */
    void startTxTransaction(EndpointState state, uint8_t bufSize=0, const uint8_t *bufPtr=nullptr) {
      // Pointer to data
      fDataPtr = (uint8_t*)bufPtr;

      // Count of bytes transferred
      fDataTransferred = 0;

      // Count of remaining bytes
      fDataRemaining = bufSize;

      this->state = state;

      // Configure the BDT for transfer
      initialiseBdtTx();
   }

   /**
    * Configure the BDT for next IN [Transmit, device -> host]
    */
    void initialiseBdtTx() {
      // Get BDT to use
      BdtEntry *bdt = txOdd?&endPointBdts[ENDPOINT_NUM].txOdd:&endPointBdts[ENDPOINT_NUM].txEven;

      if ((endPointBdts[ENDPOINT_NUM].txEven.u.bits&BDTEntry_OWN_MASK) ||
          (endPointBdts[ENDPOINT_NUM].txOdd.u.bits&BDTEntry_OWN_MASK)) {
         PRINTF("Opps-Tx\n");
      }
      uint16_t size = fDataRemaining;
      if (size > EP_MAXSIZE) {
         size = EP_MAXSIZE;
      }
      // No ZLP needed if sending undersize packet
      if (size<EP_MAXSIZE) {
         fNeedZLP = false;
      }
      // fDataBuffer may be nullptr to indicate using fDataBuffer directly
      if (fDataPtr != nullptr) {
         // Copy the Transmit data to EP buffer
         (void) memcpy(fDataBuffer, (void*)fDataPtr, size);
         // Pointer to _next_ data
         fDataPtr += size;
      }
      // Count of transferred bytes
      fDataTransferred += size;

      // Count of remaining bytes
      fDataRemaining   -= size;

      // Set up to Transmit packet
      bdt->bc = (uint8_t)size;
      if (txData1) {
         bdt->u.bits = BDTEntry_OWN_MASK|BDTEntry_DATA1_MASK|BDTEntry_DTS_MASK;
      }
      else {
         bdt->u.bits = BDTEntry_OWN_MASK|BDTEntry_DATA0_MASK|BDTEntry_DTS_MASK;
      }
   }

   /**
    *  Start an OUT transaction [Receive, device <- host, DATA0/1]
    *
    *   @param state   - State to adopt for transaction e.g. EPIdle, EPDataOut, EPStatusOut
    *   @param bufSize - Size of data to transfer (may be zero)
    *   @param bufPtr  - Buffer for data (may be nullptr)
    *
    *   @note The end-point is configured to to accept EP_MAXSIZE packet irrespective of bufSize
    */
    void startRxTransaction( EndpointState state, uint8_t bufSize=0, uint8_t *bufPtr=nullptr ) {
      fDataTransferred     = 0;        // Count of bytes transferred
      fDataRemaining       = bufSize;  // Total bytes to Receive
      fDataPtr             = bufPtr;   // Where to (eventually) place data
      this->state          = state;    // State to adopt
      initialiseBdtRx();               // Configure the BDT for transfer
   }

   /**
    * Configure the BDT for OUT [Receive, device <- host, DATA0/1]
    *
    * @note No action is taken if already configured
    * @note Always uses EP_MAXSIZE for packet size accepted
    */
    void initialiseBdtRx() {
      // Set up to Receive packet
      BdtEntry *bdt = rxOdd?&endPointBdts[ENDPOINT_NUM].rxOdd:&endPointBdts[ENDPOINT_NUM].rxEven;

      if (bdt->u.bits&BDTEntry_OWN_MASK) {
         // Already configured
         return;
      }
      if ((endPointBdts[ENDPOINT_NUM].rxEven.u.bits&BDTEntry_OWN_MASK) ||
          (endPointBdts[ENDPOINT_NUM].rxOdd.u.bits&BDTEntry_OWN_MASK)) {
         PRINTF("Opps-Rx\n");
      }
      // Set up to Receive packet
      // Always used maximum size even if expecting less data
      bdt->bc = EP_MAXSIZE;
      if (rxData1) {
         bdt->u.bits  = BDTEntry_OWN_MASK|BDTEntry_DATA1_MASK|BDTEntry_DTS_MASK;
      }
      else {
         bdt->u.bits  = BDTEntry_OWN_MASK|BDTEntry_DATA0_MASK|BDTEntry_DTS_MASK;
      }
   }

   /**
    *  Save the data from an OUT packet and advance pointers etc.
    *
    *  @return Number of bytes saved
    */
    uint8_t saveRxData() {
      // Get BDT
      BdtEntry *bdt = (!rxOdd)?&endPointBdts[ENDPOINT_NUM].rxOdd:&endPointBdts[ENDPOINT_NUM].rxEven;
      uint8_t size = bdt->bc;

      if (size > 0) {
         // Check if more data than requested - discard excess
         if (size > fDataRemaining) {
            size = fDataRemaining;
         }
         // Check if external buffer in use
         if (fDataPtr != nullptr) {
            // Copy the data from the Receive buffer to external buffer
            ( void )memcpy((void*)fDataPtr, (void*)fDataBuffer, size);
            // Advance buffer ptr
            fDataPtr    += size;
         }
         // Count of transferred bytes
         fDataTransferred += size;
         // Count down bytes to go
         fDataRemaining   -= size;
      }
      else {
         PRINTF("RxSize = 0\n");
      }
      return size;
   }

   /**
    * Handle OUT [Receive, device <- host, DATA0/1]
    */
    void handleOutToken() {
      uint8_t transferSize = 0;
//      pushState('O');

      // Toggle DATA0/1 for next pkt
      rxData1 = !rxData1;

      switch (state) {
         case EPDataOut:        // Receiving a sequence of OUT packets
            // Save the data from the Receive buffer
            transferSize = saveRxData();
//            if (fEndpointNumber == 1) {
//               PRINTF("ep1.handleOutToken() s=%d. size=%d, r=%d\n", state, transferSize, fDataRemaining);
//            }
            // Complete transfer on undersize packet or received expected number of bytes
            if ((transferSize < EP_MAXSIZE) || (fDataRemaining == 0)) {
               // Now idle
               state = EPIdle;
               doCallback(EPDataOut);
            }
            else {
               // Set up for next OUT packet
               initialiseBdtRx();
            }
            break;

         case EPStatusOut:       // Done OUT packet as a status handshake from host (IN CONTROL transfer)
            // No action
            state = EPIdle;
            doCallback(EPStatusOut);
            break;

         // We don't expect an OUT token while in the following states
         case EPDataIn:    // Doing a sequence of IN packets (until data count <= EP_MAXSIZE)
         case EPStatusIn:  // Just done an IN packet as a status handshake
         case EPIdle:      // Idle
         case EPComplete:  // Not used
         case EPStall:     // Not used
         case EPThrottle:  // Not used
            PRINTF("Unexpected OUT, s = %d\n", state);
            state = EPIdle;
            break;
      }
   }
   /**
    * Handle IN token [Transmit, device -> host]
    */
    void handleInToken() {
      // Toggle DATA0/1 for next packet
      txData1 = !txData1;

      //   PUTS(fHardwareState[BDM_OUT_ENDPOINT].data0_1?"ep2HandleInToken-T-1\n":"ep2HandleInToken-T-0\n");

      switch (state) {
         case EPDataIn:    // Doing a sequence of IN packets
            // Check if packets remaining
            if ((fDataRemaining > 0) || fNeedZLP) {
               // Set up next IN packet
               initialiseBdtTx();
            }
            else {
               state = EPIdle;
               // Execute callback function to process previous OUT data
               doCallback(EPDataIn);
            }
            break;


         case EPStatusIn: // Just done an IN packet as a status handshake for an OUT Data transfer
            // Now Idle
            state = EPIdle;
            // Execute callback function to process previous OUT data
            doCallback(EPStatusIn);
            break;

            // We don't expect an IN token while in the following states
         case EPIdle:      // Idle (Transmit complete)
         case EPDataOut:   // Doing a sequence of OUT packets (until data count <= EP_MAXSIZE)
         case EPStatusOut: // Doing an OUT packet as a status handshake
         default:
            PRINTF("Unexpected IN, %d\n", state);
            state = EPIdle;
            break;
      }
   }
};

/**
 * Class for CONTROL endpoint
 *
 * @tparam ENDPOINT_NUM Endpoint number
 * @tparam EP0_SIZE   Maximum size of packet
 */
template<class Info, int EP0_SIZE>
class ControlEndpoint : public Endpoint_T<Info, 0, EP0_SIZE> {

public:
   using Endpoint_T<Info, 0, EP0_SIZE>::txOdd;
   using Endpoint_T<Info, 0, EP0_SIZE>::usb;
   using Endpoint_T<Info, 0, EP0_SIZE>::startTxTransaction;

   /**
    * Constructor
    */
   constexpr ControlEndpoint() {
   }

   /**
    * Destructor
    */
   virtual ~ControlEndpoint() {
   }

   /**
    * Initialise endpoint
    *  - Internal state
    *  - BDTs
    *  - usb->ENDPOINT[].ENDPT
    */
    void initialise() {
      Endpoint_T<Info, 0, EP0_SIZE>::initialise();
      // Receive/Transmit/SETUP
      usb->ENDPOINT[0].ENDPT = USB_ENDPT_EPRXEN_MASK|USB_ENDPT_EPTXEN_MASK|USB_ENDPT_EPHSHK_MASK;
   }

   /**
    * Stall EP0\n
    * This stall is cleared on the next transmission
    */
   virtual void stall() {
      PRINTF("stall\n");
      // Stall Transmit only
      BdtEntry *bdt = txOdd?&endPointBdts[0].txOdd:&endPointBdts[0].txEven;
      bdt->u.bits = BDTEntry_OWN_MASK|BDTEntry_STALL_MASK|BDTEntry_DTS_MASK;
   }

   /*
    * Clear Stall on endpoint
    */
   virtual void clearStall() {
//      PRINTF("clearStall\n");
      // Release BDT as SIE doesn't
      BdtEntry *bdt = txOdd?&endPointBdts[0].txOdd:&endPointBdts[0].txEven;
      bdt->u.bits   = 0;
      Endpoint_T<Info, 0, EP0_SIZE>::clearStall();
   }

   /**
    * Start transmission of an empty status packet
    */
    void startTxStatus() {
      startTxTransaction(0, nullptr);
   }
    /**
     * Modifies endpoint state after SETUP has been received
     *
     * state == EPIdle, Toggle == DATA1
     */
    void setupReceived() {
       this->state   = EPIdle;
       this->txData1 = DATA1;
       this->rxData1 = DATA1;
    }
};

/**
 * Class for IN endpoint
 *
 * @tparam Info         Class describing associated USB hardware
 * @tparam ENDPOINT_NUM Endpoint number
 * @tparam EP_MAXSIZE   Maximum size of packet
 */
template<class Info, int ENDPOINT_NUM, int EP_MAXSIZE>
class InEndpoint : public Endpoint_T<Info, ENDPOINT_NUM, EP_MAXSIZE> {

protected:
   using Endpoint_T<Info, ENDPOINT_NUM, EP_MAXSIZE>::usb;

private:
   // Make private
   using Endpoint_T<Info, ENDPOINT_NUM, EP_MAXSIZE>::startRxTransaction;

public:
   /**
    * Constructor
    */
   constexpr InEndpoint() {
   }

   /**
    * Initialise endpoint
    *  - Internal state
    *  - BDTs
    *  - usb->ENDPOINT[].ENDPT
    */
    void initialise() {
      Endpoint_T<Info, ENDPOINT_NUM, EP_MAXSIZE>::initialise();
      // Transmit only
      usb->ENDPOINT[ENDPOINT_NUM].ENDPT = USB_ENDPT_EPTXEN_MASK|USB_ENDPT_EPHSHK_MASK;
   }
};

/**
 * Class for OUT endpoint
 *
 * @tparam Info         Class describing associated USB hardware
 * @tparam ENDPOINT_NUM Endpoint number
 * @tparam EP_MAXSIZE   Maximum size of packet
 */
template<class Info, int ENDPOINT_NUM, int EP_MAXSIZE>
class OutEndpoint : public Endpoint_T<Info, ENDPOINT_NUM, EP_MAXSIZE> {

protected:
   using Endpoint_T<Info, ENDPOINT_NUM, EP_MAXSIZE>::usb;

private:
   // Make private
   using Endpoint_T<Info, ENDPOINT_NUM, EP_MAXSIZE>::startTxTransaction;

public:
   /**
    * Constructor
    */
   constexpr OutEndpoint() {
   }

   /**
    * Initialise endpoint
    *  - Internal state
    *  - BDTs
    *  - usb->ENDPOINT[].ENDPT
    */
    void initialise() {
      Endpoint_T<Info, ENDPOINT_NUM, EP_MAXSIZE>::initialise();
      // Receive only
      usb->ENDPOINT[ENDPOINT_NUM].ENDPT = USB_ENDPT_EPRXEN_MASK|USB_ENDPT_EPHSHK_MASK;
   }
};
}; // end namespace

#endif /* PROJECT_HEADERS_USB_ENDPOINT_H_ */
