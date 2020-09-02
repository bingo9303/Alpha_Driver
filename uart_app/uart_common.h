#ifndef   _SERIALPORT_H_      
#define   _SERIALPORT_H_   


#ifdef __cplusplus  
extern "C" {  
#endif  

#define OEPN_UART_BLOCK			0		//������
#define OEPN_UART_NOBLOCK		1		//��������
		
  
int open_uart_port(int port,int flag);  
int set_uart_port(int fd,int iBaudRate,int iDataSize,char cParity,int iStopBit);  
int read_uart_port(int fd,void *buf,int iByte);  
int write_uart_port(int fd,void *buf,int iByte);  
int close_uart_port(int fd);  
  
#ifdef __cplusplus  
}  
#endif 

#endif

