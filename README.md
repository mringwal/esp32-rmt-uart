# Implement UART with Remote Control Transceiver (RMT)

Using the RMT to implement an UART allows to use the built-in carrier to modulate an UART over 
common 38 kHz Infrared Light. The moduleated 38 kHz signal can be decoded with the popular TSOP 1738 receiver.

## RMT Encoder
  - state 0: copy 'start bit' == 2 x half bit time with level 0
  - state 1: encode byte, LSB first
  - state 2: copy 'stop bit' == 2 x hafl bit time with level 1

### Example
The example sends a simple message. If USE_IR_CARRIER is enabled in main/rmt_uart_example, the UART is
modulated onto an 38 kHz carrier signal. Here, sending a '0' is translated into a 38 kHz burst of the 
configured baud rate.

