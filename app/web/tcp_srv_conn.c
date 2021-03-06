/******************************************************************************
 * FileName: tcp_srv_conn.c
 * TCP �������� ��� ESP8266
 * PV` ver1.0 20/12/2014
 ******************************************************************************/
#include "user_config.h"
#include "bios.h"
#include "add_sdk_func.h"
#include "osapi.h"
#include "user_interface.h"
#include "lwip/tcp.h"
#include "lwip/tcp_impl.h"
#include "lwip/memp.h"
#include "flash_eep.h"
#include "tcp_srv_conn.h"
#include "web_iohw.h"

#include "wifi.h"

// Lwip funcs - http://www.ecoscentric.com/ecospro/doc/html/ref/lwip.html

TCP_SERV_CFG *phcfg = NULL; // ��������� ��������� � ������ �� ��������� �������� ���������

#define mMIN(a, b)  ((a<b)?a:b)
// ����.��������...
static void tcpsrv_list_delete(TCP_SERV_CONN * ts_conn) ICACHE_FLASH_ATTR;
static void tcpsrv_disconnect_successful(TCP_SERV_CONN * ts_conn) ICACHE_FLASH_ATTR;
static void tcpsrv_close_cb(TCP_SERV_CONN * ts_conn) ICACHE_FLASH_ATTR;
static void tcpsrv_server_close(TCP_SERV_CONN * ts_conn) ICACHE_FLASH_ATTR;
static err_t tcpsrv_server_poll(void *arg, struct tcp_pcb *pcb) ICACHE_FLASH_ATTR;
static void tcpsrv_server_err(void *arg, err_t err) ICACHE_FLASH_ATTR;
static err_t tcpsrv_connected(void *arg, struct tcp_pcb *tpcb, err_t err) ICACHE_FLASH_ATTR;
static void tcpsrv_client_err(void *arg, err_t err) ICACHE_FLASH_ATTR;

/******************************************************************************
 * FunctionName : tcpsrv_print_remote_info
 * Description  : ������� remote_ip:remote_port [conn_count] os_printf("srv x.x.x.x:x [n] ")
 * Parameters   : TCP_SERV_CONN * ts_conn
 * Returns      : none
 *******************************************************************************/
void ICACHE_FLASH_ATTR tcpsrv_print_remote_info(TCP_SERV_CONN *ts_conn) {
//#if DEBUGSOO > 0
	uint16 port;
	if(ts_conn->pcb != NULL) port = ts_conn->pcb->local_port;
	else port = ts_conn->pcfg->port;
	os_printf("srv[%u] " IPSTR ":%d [%d] ", port,
			ts_conn->remote_ip.b[0], ts_conn->remote_ip.b[1],
			ts_conn->remote_ip.b[2], ts_conn->remote_ip.b[3],
			ts_conn->remote_port, ts_conn->pcfg->conn_count);
//#endif
}
/******************************************************************************
 * Demo functions
 ******************************************************************************/
//------------------------------------------------------------------------------
void ICACHE_FLASH_ATTR tcpsrv_disconnect_calback_default(TCP_SERV_CONN *ts_conn) {
	ts_conn->pcb = NULL;
#if DEBUGSOO > 1
	tcpsrv_print_remote_info(ts_conn);
	os_printf("disconnect\n");
#endif
}
//------------------------------------------------------------------------------
err_t ICACHE_FLASH_ATTR tcpsrv_listen_default(TCP_SERV_CONN *ts_conn) {
#if DEBUGSOO > 1
	tcpsrv_print_remote_info(ts_conn);
	os_printf("listen\n");
#endif
	return ERR_OK;
}
//------------------------------------------------------------------------------
err_t ICACHE_FLASH_ATTR tcpsrv_connected_default(TCP_SERV_CONN *ts_conn) {
#if DEBUGSOO > 1
	tcpsrv_print_remote_info(ts_conn);
	os_printf("connected\n");
#endif
	return ERR_OK;
}
//------------------------------------------------------------------------------
err_t ICACHE_FLASH_ATTR tcpsrv_sent_callback_default(TCP_SERV_CONN *ts_conn) {
#if DEBUGSOO > 1
	tcpsrv_print_remote_info(ts_conn);
	os_printf("sent_cb\n");
#endif
	return ERR_OK;
}
//------------------------------------------------------------------------------
err_t ICACHE_FLASH_ATTR tcpsrv_received_data_default(TCP_SERV_CONN *ts_conn) {
#if DEBUGSOO > 1
	tcpsrv_print_remote_info(ts_conn);
	os_printf("received, buffer %d bytes\n", ts_conn->sizei);
#endif
	return ERR_OK;
}
/******************************************************************************
 * FunctionName : find_pcb
 * Description  : ����� pcb � ������� lwip
 * Parameters   : TCP_SERV_CONN * ts_conn
 * Returns      : *pcb or NULL
 *******************************************************************************/
struct tcp_pcb * ICACHE_FLASH_ATTR find_tcp_pcb(TCP_SERV_CONN * ts_conn) {
	struct tcp_pcb *pcb;
	uint16 remote_port = ts_conn->remote_port;
	uint16 local_port = ts_conn->pcfg->port;
	uint32 ip = ts_conn->remote_ip.dw;
	for (pcb = tcp_active_pcbs; pcb != NULL; pcb = pcb->next) {
		if ((pcb->remote_port == remote_port) && (pcb->local_port == local_port)
				&& (pcb->remote_ip.addr == ip))
			return pcb;
	}
	for (pcb = tcp_tw_pcbs; pcb != NULL; pcb = pcb->next) {
		if ((pcb->remote_port == remote_port) && (pcb->local_port == local_port)
				&& (pcb->remote_ip.addr == ip))
			return pcb;
	}
	return NULL;
}
/******************************************************************************
 * FunctionName : tcpsrv_disconnect
 * Description  : disconnect
 * Parameters   : TCP_SERV_CONN * ts_conn
 * Returns      : none
 *******************************************************************************/
void ICACHE_FLASH_ATTR tcpsrv_disconnect(TCP_SERV_CONN * ts_conn) {
	if (ts_conn == NULL || ts_conn->state == SRVCONN_CLOSEWAIT) return; // ��� �����������
	ts_conn->pcb = find_tcp_pcb(ts_conn); // ��� ���� ������ pcb ?
	if (ts_conn->pcb != NULL) {
		tcpsrv_server_close(ts_conn);
	}
}
/******************************************************************************
 * FunctionName : internal fun: tcpsrv_int_sent_data
 * Description  : �������� ������ (�� ����������������! ������ �������� � tcp Lwip-�)
 * 				  �������� ������ �� call back � ������� pcb!
 * Parameters   : TCP_SERV_CONN * ts_conn
 *                uint8* psent - ����� � �������
 *                uint16 length - ���-�� ������������ ����
 * Returns      : tcp error
 ******************************************************************************/
err_t ICACHE_FLASH_ATTR tcpsrv_int_sent_data(TCP_SERV_CONN * ts_conn, uint8 *psent, uint16 length) {
	err_t err = ERR_ARG;
	if(ts_conn == NULL) return err;
	if(ts_conn->pcb == NULL || ts_conn->state == SRVCONN_CLOSEWAIT) return ERR_CONN;
	ts_conn->flag.busy_bufo = 1; // ����� bufo �����
	struct tcp_pcb *pcb = ts_conn->pcb;  // find_tcp_pcb(ts_conn);
	if(tcp_sndbuf(pcb) < length) {
#if DEBUGSOO > 1
		os_printf("sent_data length (%u) > sndbuf (%u)!\n", length, tcp_sndbuf(pcb));
#endif
		return err;
	}
	if (length) {
		if(ts_conn->flag.nagle_disabled) tcp_nagle_disable(pcb);
		err = tcp_write(pcb, psent, length, 0);
		if (err == ERR_OK) {
			ts_conn->ptrtx = psent + length;
			ts_conn->cntro -= length;
			ts_conn->flag.wait_sent = 1; // ������� ���������� �������� (sent_cb)
			err = tcp_output(pcb); // �������� ��� ������
		} else { // ts_conn->state = SRVCONN_CLOSE;
#if DEBUGSOO > 1
			os_printf("tcp_write(%p, %p, %u) = %d! pbuf = %u\n", pcb, psent, length, err, tcp_sndbuf(pcb));
#endif
			ts_conn->flag.wait_sent = 0;
			tcpsrv_server_close(ts_conn);
		};
	} else { // ������� ����� tcpsrv_server_sent()
		tcp_nagle_enable(pcb);
		err = tcp_output(pcb); // �������� ������
	}
	ts_conn->flag.busy_bufo = 0; // ����� bufo ��������
	return err;
}
/******************************************************************************
 * FunctionName : tcpsrv_server_sent
 * Description  : Data has been sent and acknowledged by the remote host.
 * This means that more data can be sent.
 * Parameters   : arg -- Additional argument to pass to the callback function
 *                pcb -- The connection pcb for which data has been acknowledged
 *                len -- The amount of bytes acknowledged
 * Returns      : ERR_OK: try to send some data by calling tcp_output
 *                ERR_ABRT: if you have called tcp_abort from within the function!
 ******************************************************************************/
static err_t ICACHE_FLASH_ATTR tcpsrv_server_sent(void *arg, struct tcp_pcb *pcb, u16_t len) {
	sint8 ret_err = ERR_OK;
	TCP_SERV_CONN * ts_conn = arg;
	if (ts_conn == NULL || pcb == NULL)	return ERR_ARG;
	ts_conn->pcb = pcb; // ��������� pcb
	ts_conn->state = SRVCONN_CONNECT;
	ts_conn->recv_check = 0;
	ts_conn->flag.wait_sent = 0; // ���� �������
	if ((ts_conn->flag.tx_null == 0)
			&& (ts_conn->pcfg->func_sent_cb != NULL)) {
		ret_err = ts_conn->pcfg->func_sent_cb(ts_conn);
	}
	return ret_err;
}
/******************************************************************************
 * FunctionName : tcpsrv_server_recv
 * Description  : Data has been received on this pcb.
 * Parameters   : arg -- Additional argument to pass to the callback function
 *                pcb -- The connection pcb which received data
 *                p -- The received data (or NULL when the connection has been closed!)
 *                err -- An error code if there has been an error receiving
 * Returns      : ERR_ABRT: if you have called tcp_abort from within the function!
 ******************************************************************************/
static err_t ICACHE_FLASH_ATTR tcpsrv_server_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err) {
	// Sets the callback function that will be called when new data arrives on the connection associated with pcb.
	// The callback function will be passed a NULL pbuf to indicate that the remote host has closed the connection.
	TCP_SERV_CONN * ts_conn = arg;
	if (ts_conn == NULL) return ERR_ARG;

	if(syscfg.cfg.b.hi_speed_enable) set_cpu_clk();

	ts_conn->pcb = pcb;
	if (p == NULL || err != ERR_OK) { // the remote host has closed the connection.
		tcpsrv_server_close(ts_conn);
		return err;
	};
	// ���� ��� ������� ��������� ��� ������� �������� ����������, �� ��������� ������ � �����
	if ((ts_conn->flag.rx_null != 0) || (ts_conn->pcfg->func_recv == NULL)
			|| (ts_conn->state == SRVCONN_CLOSEWAIT)) { // ���������� �������? ���.
		tcp_recved(pcb, p->tot_len + ts_conn->unrecved_bytes); // �������� �����, ��� ����� len � ����� �������� ACK � ��������� ����� ������.
		ts_conn->unrecved_bytes = 0;
#if DEBUGSOO > 3
		os_printf("rec_null %d of %d\n", ts_conn->cntri, p->tot_len);
#endif
		pbuf_free(p); // ������ �����
		return ERR_OK;
	};
	ts_conn->state = SRVCONN_CONNECT; // ��� �����
	ts_conn->recv_check = 0; // ���� ������� �� ����-�������� � ����

	int len = 0;
	ts_conn->flag.busy_bufi = 1; // ���� ��������� bufi
	if(ts_conn->pbufi != NULL && (ts_conn->flag.rx_buf) && ts_conn->cntri < ts_conn->sizei) {
			len = ts_conn->sizei - ts_conn->cntri;
	}
	else {
		os_free(ts_conn->pbufi);
		ts_conn->pbufi = NULL;
	}
	{
		uint8 * newbufi = (uint8 *) os_malloc(len + p->tot_len + 1); // ���������� ������
	#if DEBUGSOO > 2
		os_printf("memi[%d] %p ", len + p->tot_len, ts_conn->pbufi);
	#endif
		if (newbufi == NULL) {
			ts_conn->flag.busy_bufi = 0; // ��������� bufi ��������
			return ERR_MEM;
		}
		newbufi[len + p->tot_len] = '\0'; // ������ os_zalloc
		if(len)	{
			os_memcpy(newbufi, &ts_conn->pbufi[ts_conn->cntri], len);
			os_free(ts_conn->pbufi);
		};
		ts_conn->pbufi = newbufi;
		ts_conn->cntri = 0;
		ts_conn->sizei = len;
	};
	// ���������� ����� ��� ����� Lwip � �����
	len = pbuf_copy_partial(p, &ts_conn->pbufi[len], p->tot_len, 0);

	ts_conn->sizei += len;
	pbuf_free(p); // ��� ������ �����
	if(!ts_conn->flag.rx_buf) {
		 tcp_recved(pcb, len); // �������� �����, ��� ����� len � ����� �������� ACK � ��������� ����� ������.
	}
	else ts_conn->unrecved_bytes += len;
#if DEBUGSOO > 3
	os_printf("rec %d of %d :\n", p->tot_len, ts_conn->sizei);
#endif
	err = ts_conn->pcfg->func_recv(ts_conn);
	if((!ts_conn->flag.rx_buf) || ts_conn->cntri >= ts_conn->sizei)  {
		ts_conn->sizei = 0;
		if (ts_conn->pbufi != NULL) {
			os_free(ts_conn->pbufi);  // ���������� �����.
			ts_conn->pbufi = NULL;
		};
	}
	else if(ts_conn->cntri) {
		len = ts_conn->sizei - ts_conn->cntri;
		os_memcpy(ts_conn->pbufi, &ts_conn->pbufi[ts_conn->cntri], len );
		ts_conn->sizei = len;
		ts_conn->pbufi = (uint8 *)mem_realloc(ts_conn->pbufi, len);	//mem_trim(ts_conn->pbufi, len);
	}
	ts_conn->cntri = 0;
	ts_conn->flag.busy_bufi = 0; // ��������� bufi ��������
	return err;
}
/******************************************************************************
 * FunctionName : tcpsrv_unrecved_win
 * Description  : Update the TCP window.
 * This can be used to throttle data reception (e.g. when received data is
 * programmed to flash and data is received faster than programmed).
 * Parameters   : TCP_SERV_CONN * ts_conn
 * Returns      : none
 * ����� ��������� throttle, ����� ������� �� 5840 (MAX WIN) + 1460 (MSS) ����?
 ******************************************************************************/
void ICACHE_FLASH_ATTR tcpsrv_unrecved_win(TCP_SERV_CONN *ts_conn) {
	if (ts_conn->unrecved_bytes) {
		// update the TCP window
#if DEBUGSOO > 3
		os_printf("recved_bytes=%d\n", ts_conn->unrecved_bytes);
#endif
#if 1
		if(ts_conn->pcb != NULL) tcp_recved(ts_conn->pcb, ts_conn->unrecved_bytes);
#else
		struct tcp_pcb *pcb = find_tcp_pcb(ts_conn); // ��� �������?
		if(pcb != NULL)	tcp_recved(ts_conn->pcb, ts_conn->unrecved_bytes);
#endif
	}
	ts_conn->unrecved_bytes = 0;
}
/******************************************************************************
 * FunctionName : tcpsrv_disconnect
 * Description  : disconnect with host
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
 ******************************************************************************/
static void ICACHE_FLASH_ATTR tcpsrv_disconnect_successful(TCP_SERV_CONN * ts_conn) {
	struct tcp_pcb *pcb = ts_conn->pcb;
	if (pcb != NULL && ts_conn->flag.pcb_time_wait_free && pcb->state == TIME_WAIT) { // ����� TIME_WAIT?
		// http://www.serverframework.com/asynchronousevents/2011/01/time-wait-and-its-design-implications-for-protocols-and-scalable-servers.html
#if DEBUGSOO > 3
		tcpsrv_print_remote_info(ts_conn);
		os_printf("tcp_pcb_remove!\n");
#endif
		tcp_pcb_remove(&tcp_tw_pcbs, pcb);
		memp_free(MEMP_TCP_PCB, pcb);
	};
	// remove the node from the server's connection list
	tcpsrv_list_delete(ts_conn);
}
/******************************************************************************
 * FunctionName : tcpsrv_close_cb
 * Description  : The connection has been successfully closed.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
 ******************************************************************************/
static void ICACHE_FLASH_ATTR tcpsrv_close_cb(TCP_SERV_CONN * ts_conn) {
//	if (ts_conn == NULL) return;
	struct tcp_pcb *pcb = find_tcp_pcb(ts_conn); // ts_conn->pcb;
	ts_conn->pcb = pcb;
	if (pcb == NULL || pcb->state == CLOSED || pcb->state == TIME_WAIT) {
		/*remove the node from the server's active connection list*/
		tcpsrv_disconnect_successful(ts_conn);
	} else {
		if (++ts_conn->recv_check > TCP_SRV_CLOSE_WAIT) { // ���� �� ��������������� ��������  120*0.25 = 30 c��
#if DEBUGSOO > 2
			tcpsrv_print_remote_info(ts_conn);
			os_printf("tcp_abandon!\n");
#endif
			tcp_poll(pcb, NULL, 0);
			tcp_err(pcb, NULL);
			tcp_abandon(pcb, 0);
			ts_conn->pcb = NULL;
			// remove the node from the server's active connection list
			tcpsrv_disconnect_successful(ts_conn);
		} else
			os_timer_arm(&ts_conn->ptimer, TCP_FAST_INTERVAL*2, 0); // ���� ��� 250 ms
	}
}
/******************************************************************************
 * FunctionName : tcpsrv_server_close
 * Description  : The connection shall be actively closed.
 * Parameters   : arg -- Additional argument to pass to the callback function
 *                pcb -- the pcb to close
 * Returns      : none
 ******************************************************************************/
static void ICACHE_FLASH_ATTR tcpsrv_server_close(TCP_SERV_CONN * ts_conn) {

	struct tcp_pcb *pcb = ts_conn->pcb;
//	if(pcb == NULL) return;
	ts_conn->state = SRVCONN_CLOSEWAIT;
	ts_conn->recv_check = 0;

	ts_conn->flag.wait_sent = 0; // ���� �������
	ts_conn->flag.rx_null = 1; // ���������� ������ func_received_data() � ����� � null
	ts_conn->flag.tx_null = 1; // ���������� ������ func_sent_callback() � �������� � null
	tcp_recv(pcb, NULL); // ���������� ������
	tcp_sent(pcb, NULL); // ���������� ��������
	tcp_poll(pcb, NULL, 0); // ���������� poll
	if (ts_conn->pbufo != NULL) {
		os_free(ts_conn->pbufo);
		ts_conn->pbufo = NULL;
	}
	ts_conn->sizeo = 0;
	ts_conn->cntro = 0;
	if (ts_conn->pbufi != NULL)	{
		os_free(ts_conn->pbufi);
		ts_conn->pbufi = NULL;
	}
	ts_conn->sizei = 0;
	ts_conn->cntri = 0;
	if(ts_conn->unrecved_bytes) {
		tcp_recved(ts_conn->pcb, ts_conn->unrecved_bytes);
		ts_conn->unrecved_bytes = 0;
	}
	err_t err = tcp_close(pcb); // ������� �������� ����������
	// The function may return ERR_MEM if no memory was available for closing the connection.
	// If so, the application should wait and try again either by using the acknowledgment callback or the polling functionality.
	// If the close succeeds, the function returns ERR_OK.
	os_timer_disarm(&ts_conn->ptimer);
	os_timer_setfn(&ts_conn->ptimer, (ETSTimerFunc *)tcpsrv_close_cb, ts_conn);
	if (err != ERR_OK) {
#if DEBUGSOO > 1
		tcpsrv_print_remote_info(ts_conn);
		os_printf("+ncls+\n", pcb);
#endif
		// closing failed, try again later
	}
//	else { } // closing succeeded
	os_timer_arm(&ts_conn->ptimer, TCP_FAST_INTERVAL*2, 0); // ���� �����, �� Lwip �� �������� pcb->state = TIME_WAIT ?
}
/******************************************************************************
 * FunctionName : espconn_server_poll
 * Description  : The poll function is called every 1 second.
 * If there has been no data sent (which resets the retries) in time_wait_rec seconds, close.
 * If the last portion of a file has not been recv/sent in time_wait_cls seconds, close.
 *
 * This could be increased, but we don't want to waste resources for bad connections.
 * Parameters   : arg -- Additional argument to pass to the callback function
 *                pcb -- The connection pcb for which data has been acknowledged
 * Returns      : ERR_OK: try to send some data by calling tcp_output
 *                ERR_ABRT: if you have called tcp_abort from within the function!
 *******************************************************************************/
static err_t ICACHE_FLASH_ATTR tcpsrv_server_poll(void *arg, struct tcp_pcb *pcb) {
	TCP_SERV_CONN * ts_conn = arg;
	if (ts_conn == NULL) {
#if DEBUGSOO > 3
		os_printf("poll, ts_conn = NULL! - abandon\n");
#endif
		tcp_poll(pcb, NULL, 0);
		tcp_abandon(pcb, 0);
		return ERR_ABRT;
	}
#if DEBUGSOO > 3
	tcpsrv_print_remote_info(ts_conn);
	os_printf("poll %d #%d\n", ts_conn->recv_check, pcb->state );
#endif
	if (ts_conn->state != SRVCONN_CLOSEWAIT) {
		ts_conn->pcb = pcb;
		if (pcb->state == ESTABLISHED) {
			if (ts_conn->state == SRVCONN_LISTEN) {
				if ((ts_conn->pcfg->time_wait_rec)
					&& (++ts_conn->recv_check > ts_conn->pcfg->time_wait_rec))
					tcpsrv_server_close(ts_conn);
			}
			else if (ts_conn->state == SRVCONN_CONNECT) {
				if ((ts_conn->pcfg->time_wait_cls)
					&& (++ts_conn->recv_check > ts_conn->pcfg->time_wait_cls))
					tcpsrv_server_close(ts_conn);
			}
		} else
			tcpsrv_server_close(ts_conn);
	} else
		tcp_poll(pcb, NULL, 0); // ��������� tcpsrv_server_poll
	return ERR_OK;
}
/******************************************************************************
 * FunctionName : tcpsrv_list_delete
 * Description  : remove the node from the connection list
 * Parameters   : ts_conn
 * Returns      : none
 *******************************************************************************/
static void ICACHE_FLASH_ATTR tcpsrv_list_delete(TCP_SERV_CONN * ts_conn) {
//	if (ts_conn == NULL) return;
	if(ts_conn->state != SRVCONN_CLOSED) {
		ts_conn->state = SRVCONN_CLOSED; // ��������� ��������� ��������� �� �������� � func_discon_cb()
		if (ts_conn->pcfg->func_discon_cb != NULL)
			ts_conn->pcfg->func_discon_cb(ts_conn);
		if(phcfg == NULL || ts_conn->pcfg == NULL) return;  // ���� � func_discon_cb() ���� ������� tcpsrv_close_all() � �.�.
	}
	TCP_SERV_CONN ** p = &ts_conn->pcfg->conn_links;
	TCP_SERV_CONN *tcpsrv_cmp = ts_conn->pcfg->conn_links;
	while (tcpsrv_cmp != NULL) {
		if (tcpsrv_cmp == ts_conn) {
			*p = ts_conn->next;
			ts_conn->pcfg->conn_count--;
			os_timer_disarm(&ts_conn->ptimer);
			if (ts_conn->linkd != NULL) {
				os_free(ts_conn->linkd);
				ts_conn->linkd = NULL;
			}
			if (ts_conn->pbufo != NULL) {
				os_free(ts_conn->pbufo);
				ts_conn->pbufo = NULL;
			}
			if (ts_conn->pbufi != NULL) {
				os_free(ts_conn->pbufi);
				ts_conn->pbufi = NULL;
			}
			os_free(ts_conn);
			return; // break;
		}
		p = &tcpsrv_cmp->next;
		tcpsrv_cmp = tcpsrv_cmp->next;
	};
}
/******************************************************************************
 * FunctionName : tcpsrv_server_err
 * Description  : The pcb had an error and is already deallocated.
 *		The argument might still be valid (if != NULL).
 * Parameters   : arg -- Additional argument to pass to the callback function
 *		err -- Error code to indicate why the pcb has been closed
 * Returns      : none
 *******************************************************************************/
#if DEBUGSOO > 2
static const char srvContenErr00[] ICACHE_RODATA_ATTR = "Ok";						// ERR_OK          0
static const char srvContenErr01[] ICACHE_RODATA_ATTR = "Out of memory error";		// ERR_MEM        -1
static const char srvContenErr02[] ICACHE_RODATA_ATTR = "Buffer error";				// ERR_BUF        -2
static const char srvContenErr03[] ICACHE_RODATA_ATTR = "Timeout";					// ERR_TIMEOUT    -3
static const char srvContenErr04[] ICACHE_RODATA_ATTR = "Routing problem";			// ERR_RTE        -4
static const char srvContenErr05[] ICACHE_RODATA_ATTR = "Operation in progress";	// ERR_INPROGRESS -5
static const char srvContenErr06[] ICACHE_RODATA_ATTR = "Illegal value";			// ERR_VAL        -6
static const char srvContenErr07[] ICACHE_RODATA_ATTR = "Operation would block";	// ERR_WOULDBLOCK -7
static const char srvContenErr08[] ICACHE_RODATA_ATTR = "Connection aborted";		// ERR_ABRT       -8
static const char srvContenErr09[] ICACHE_RODATA_ATTR = "Connection reset";			// ERR_RST        -9
static const char srvContenErr10[] ICACHE_RODATA_ATTR = "Connection closed";		// ERR_CLSD       -10
static const char srvContenErr11[] ICACHE_RODATA_ATTR = "Not connected";			// ERR_CONN       -11
static const char srvContenErr12[] ICACHE_RODATA_ATTR = "Illegal argument";			// ERR_ARG        -12
static const char srvContenErr13[] ICACHE_RODATA_ATTR = "Address in use";			// ERR_USE        -13
static const char srvContenErr14[] ICACHE_RODATA_ATTR = "Low-level netif error";	// ERR_IF         -14
static const char srvContenErr15[] ICACHE_RODATA_ATTR = "Already connected";		// ERR_ISCONN     -15
static const char srvContenErrX[] ICACHE_RODATA_ATTR = "?";
const char * srvContenErr[]  =  {
	srvContenErr00,
	srvContenErr01,
	srvContenErr02,
	srvContenErr03,
	srvContenErr04,
	srvContenErr05,
	srvContenErr06,
	srvContenErr07,
	srvContenErr08,
	srvContenErr09,
	srvContenErr10,
	srvContenErr11,
	srvContenErr12,
	srvContenErr13,
	srvContenErr14,
	srvContenErr15
};
#endif
static void ICACHE_FLASH_ATTR tcpsrv_server_err(void *arg, err_t err) {
	TCP_SERV_CONN * ts_conn = arg;
//	struct tcp_pcb *pcb = NULL;
	if (ts_conn != NULL) {
#if DEBUGSOO > 2
		if(system_get_os_print()) {
			tcpsrv_print_remote_info(ts_conn);
			char serr[24];
			if((err > -16) && (err < 1)) {
				ets_memcpy(serr, srvContenErr[-err], 24);
			}
			else {
				serr[0] = '?';
				serr[1] = '\0';
			}
			os_printf("error %d (%s)\n", err, serr);
		}
#elif DEBUGSOO > 1
		tcpsrv_print_remote_info(ts_conn);
		os_printf("error %d\n", err);
#endif
		if (ts_conn->state != SRVCONN_CLOSEWAIT) {
			// remove the node from the server's connection list
			tcpsrv_list_delete(ts_conn);
		};
	}
}

/******************************************************************************
 * FunctionName : tcpsrv_client_connect
 * Returns      : ts_conn->pcb
 *******************************************************************************/
static void ICACHE_FLASH_ATTR tcpsrv_client_connect(TCP_SERV_CONN * ts_conn)
{
	if (ts_conn != NULL) {
		struct tcp_pcb *pcb;
		if(ts_conn->pcb != NULL) {
			pcb = find_tcp_pcb(ts_conn);
			if(pcb != NULL) {
				tcp_abandon(ts_conn->pcb, 0);
//				ts_conn->pcb = NULL;
			}
		}
		pcb = tcp_new();
		if(pcb != NULL) {
			ts_conn->pcb = pcb;
			err_t err = tcp_bind(pcb, &netif_default->ip_addr, 0); // Binds pcb to a local IP address and new port number.
#if DEBUGSOO > 1
			os_printf("tcp_bind() = %d\n", err);
#endif
			if (err == ERR_OK) { // If another connection is bound to the same port, the function will return ERR_USE, otherwise ERR_OK is returned.
				ts_conn->pcfg->port = ts_conn->pcb->local_port;
				tcp_arg(pcb, ts_conn); // Allocate client-specific session structure, set as callback argument
				// Set up the various callback functions
				tcp_err(pcb, tcpsrv_client_err);
				err = tcp_connect(pcb, (ip_addr_t *)&ts_conn->remote_ip, ts_conn->remote_port, tcpsrv_connected);
#if DEBUGSOO > 1
				os_printf("tcp_connect() = %d\n", err);
#endif
				if(err == ERR_OK) {
#if DEBUGSOO > 1
					tcpsrv_print_remote_info(ts_conn);
					os_printf("start client - Ok\n");
#endif
					return;
				}
			}
			tcp_abandon(pcb, 0);
		}
		ts_conn->pcb = NULL;
	}
}
/******************************************************************************
 * FunctionName : tcpsrv_client_err (connect err)
 * Description  : The pcb had an error and is already deallocated.
 *		The argument might still be valid (if != NULL).
 * Parameters   : arg -- Additional argument to pass to the callback function
 *		err -- Error code to indicate why the pcb has been closed
 * Returns      : none
 *******************************************************************************/
static void ICACHE_FLASH_ATTR tcpsrv_client_err(void *arg, err_t err) {
	TCP_SERV_CONN * ts_conn = arg;
	if (ts_conn != NULL) {
		os_timer_disarm(&ts_conn->ptimer);
		if(ts_conn->pcb != NULL) {
			ts_conn->pcb = find_tcp_pcb(ts_conn);
			if(ts_conn->pcb != NULL) {
				tcp_abandon(ts_conn->pcb, 0);
				ts_conn->pcb = NULL;
			}
		}
#if DEBUGSOO > 2
		if(system_get_os_print()) {
			tcpsrv_print_remote_info(ts_conn);
			char serr[24];
			if((err > -16) && (err < 1)) {
				ets_memcpy(serr, srvContenErr[-err], 24);
			}
			else {
				serr[0] = '?';
				serr[1] = '\0';
			}
			os_printf("error %d (%s)\n", err, serr);
		}
#elif DEBUGSOO > 1
		tcpsrv_print_remote_info(ts_conn);
		os_printf("error %d\n", err);
#endif
		if(ts_conn->state == SRVCONN_CLIENT) {
			ts_conn->recv_check++;
			if(ts_conn->pcfg->max_conn != 0 && ts_conn->recv_check < ts_conn->pcfg->max_conn) {
#if DEBUGSOO > 1
				os_printf("next tcp_connect() ...\n");
#endif
				os_timer_setfn(&ts_conn->ptimer, (ETSTimerFunc *)tcpsrv_client_connect, ts_conn);
				os_timer_arm(&ts_conn->ptimer, TCP_CLIENT_NEXT_CONNECT_MS, 0); // ����������� ����������� ����� 3 �������
			}
			else tcpsrv_list_delete(ts_conn); // remove the node from the server's connection list
		}
		else tcpsrv_list_delete(ts_conn); // remove the node from the server's connection list
	}
}
/******************************************************************************
 tcpsrv_connected_fn (client)
 *******************************************************************************/
static err_t ICACHE_FLASH_ATTR tcpsrv_connected(void *arg, struct tcp_pcb *tpcb, err_t err)
{
	TCP_SERV_CONN * ts_conn = arg;
	err_t merr = ERR_OK;
	if (ts_conn != NULL) {
		os_timer_disarm(&ts_conn->ptimer);
		tcp_err(tpcb, tcpsrv_server_err);
		ts_conn->state = SRVCONN_LISTEN;
		ts_conn->recv_check = 0;
		tcp_sent(tpcb, tcpsrv_server_sent);
		tcp_recv(tpcb, tcpsrv_server_recv);
		tcp_poll(tpcb, tcpsrv_server_poll, 8); // every 1 seconds
		if(ts_conn->pcfg->func_listen != NULL) merr = ts_conn->pcfg->func_listen(ts_conn);
		else {
#if DEBUGSOO > 2
			if(system_get_os_print()) {
				tcpsrv_print_remote_info(ts_conn);
				char serr[24];
				if((err > -16) && (err < 1)) {
					ets_memcpy(serr, srvContenErr[-err], 24);
				}
				else {
					serr[0] = '?';
					serr[1] = '\0';
				}
				os_printf("error %d (%s)\n", err, serr);
			}
#elif DEBUGSOO > 1
			tcpsrv_print_remote_info(ts_conn);
			os_printf("connected, error %d\n", err);
#endif
			tcpsrv_int_sent_data(ts_conn, wificonfig.st.config.password, os_strlen(wificonfig.st.config.password));
		}
	}
	return merr;
}
/******************************************************************************
 * FunctionName : tcpsrv_tcp_accept
 * Description  : A new incoming connection has been accepted.
 * Parameters   : arg -- Additional argument to pass to the callback function
 *		pcb -- The connection pcb which is accepted
 *		err -- An unused error code, always ERR_OK currently
 * Returns      : acception result
 *******************************************************************************/
static err_t ICACHE_FLASH_ATTR tcpsrv_server_accept(void *arg, struct tcp_pcb *pcb, err_t err) {
	struct tcp_pcb_listen *lpcb = (struct tcp_pcb_listen*) arg;
	TCP_SERV_CFG *p = tcpsrv_port2pcfg(pcb->local_port);

	if (p == NULL)	return ERR_ARG;

	if (system_get_free_heap_size() < p->min_heap) {
#if DEBUGSOO > 1
		os_printf("srv[%u] new listen - low heap size!\n", p->port);
#endif
		return ERR_MEM;
	}
	if (p->conn_count >= p->max_conn) {
#if DEBUGSOO > 1
		os_printf("srv[%u] new listen - max connection!\n", p->port);
#endif
		return ERR_CONN;
	}
	TCP_SERV_CONN * ts_conn = (TCP_SERV_CONN *) os_zalloc(sizeof(TCP_SERV_CONN));
	if (ts_conn == NULL) {
#if DEBUGSOO > 1
		os_printf("srv[%u] new listen - out of mem!\n", ts_conn->pcfg->port);
#endif
		return ERR_MEM;
	}
	ts_conn->pcfg = p;
	// ��������� ����� �� ��������� �� ������ ����������
	ts_conn->flag = p->flag;
	tcp_accepted(lpcb); // Decrease the listen backlog counter
	//  tcp_setprio(pcb, TCP_PRIO_MIN); // Set priority ?
	// init/copy data ts_conn
	ts_conn->pcb = pcb;
	ts_conn->remote_port = pcb->remote_port;
	ts_conn->remote_ip.dw = pcb->remote_ip.addr;
	ts_conn->state = SRVCONN_LISTEN;
//	*(uint16 *) &ts_conn->flag = 0; //zalloc
//	ts_conn->recv_check = 0; //zalloc
//  ts_conn->linkd = NULL; //zalloc
	// Insert new ts_conn
	ts_conn->next = ts_conn->pcfg->conn_links;
	ts_conn->pcfg->conn_links = ts_conn;
	ts_conn->pcfg->conn_count++;
	// Tell TCP that this is the structure we wish to be passed for our callbacks.
	tcp_arg(pcb, ts_conn);
	// Set up the various callback functions
	tcp_err(pcb, tcpsrv_server_err);
	tcp_sent(pcb, tcpsrv_server_sent);
	tcp_recv(pcb, tcpsrv_server_recv);
	tcp_poll(pcb, tcpsrv_server_poll, 8); /* every 1 seconds (SDK 0.9.4) */
	if (p->func_listen != NULL)
		return p->func_listen(ts_conn);
	return ERR_OK;
}
/******************************************************************************
 * FunctionName : tcpsrv_port2conn
 * Description  : ����� ������� �� �����
 * Parameters   : ����� �����
 * Returns      : ��������� �� TCP_SERV_CFG ��� NULL
 *******************************************************************************/
TCP_SERV_CFG * ICACHE_FLASH_ATTR tcpsrv_port2pcfg(uint16 portn) {
	TCP_SERV_CFG * p;
	for (p = phcfg; p != NULL; p = p->next)
		if (p->port == portn)
			return p;
	return NULL;
}
/******************************************************************************
 tcpsrv_init server or client.
 client -> port = 1 ?
 *******************************************************************************/
TCP_SERV_CFG * ICACHE_FLASH_ATTR tcpsrv_init(uint16 portn) {
	//	if (portn == 0)	portn = 80;
	if (portn == 0)	return NULL;
#if DEBUGSOO > 0
	if(portn == ID_CLIENTS_PORT) {
		os_printf("\nTCP Client service init ");
	}
	else os_printf("\nTCP Server init on port %u - ", portn);
#endif
	TCP_SERV_CFG * p;
	for (p = phcfg; p != NULL; p = p->next) {
		if (p->port == portn) {
#if DEBUGSOO > 0
			os_printf("already initialized!\n");
#endif
			return NULL;
		}
	}
	p = (TCP_SERV_CFG *) os_zalloc(sizeof(TCP_SERV_CFG));
	if (p == NULL) {
#if DEBUGSOO > 0
		os_printf("out of memory!\n");
#endif
		return NULL;
	}
	p->port = portn;
	p->conn_count = 0;
	p->min_heap = TCP_SRV_MIN_HEAP_SIZE;
	p->time_wait_rec = TCP_SRV_RECV_WAIT;
	p->time_wait_cls = TCP_SRV_END_WAIT;
	// p->phcfg->conn_links = NULL; // zalloc
	// p->pcb = NULL; // zalloc
	// p->lnk = NULL; // zalloc
	if(portn != ID_CLIENTS_PORT) {
		p->max_conn = TCP_SRV_MAX_CONNECTIONS;
		p->func_listen = tcpsrv_listen_default;
	}
	else {
		p->max_conn = TCP_CLIENT_MAX_CONNECT_RETRY;
		p->flag.client = 1; // ������ ���������� �� ������, � ������!
		// insert new tcpsrv_config
		p->next = phcfg;
		phcfg = p;
	}
	p->func_discon_cb = tcpsrv_disconnect_calback_default;
	p->func_sent_cb = tcpsrv_sent_callback_default;
	p->func_recv = tcpsrv_received_data_default;
#if DEBUGSOO > 0
	os_printf("Ok\n");
#endif
#if DEBUGSOO > 2
	os_printf("struct size: %d %d\n", sizeof(TCP_SERV_CFG),
			sizeof(TCP_SERV_CONN));
#endif
	return p;
}
/******************************************************************************
 tcpsrv_start
 *******************************************************************************/
err_t ICACHE_FLASH_ATTR tcpsrv_start(TCP_SERV_CFG *p) {
	err_t err = ERR_OK;
#if DEBUGSOO > 0
	os_printf("TCP Server start - ");
#endif
	if (p == NULL) {
#if DEBUGSOO > 0
		os_printf("NULL pointer!\n");
#endif
		return false;
	}
	if (p->pcb != NULL) {
#if DEBUGSOO > 0
		os_printf("already running!\n");
#endif
		return false;
	}
	p->pcb = tcp_new();
	if (p->pcb != NULL) {
		err = tcp_bind(p->pcb, IP_ADDR_ANY, p->port); // Binds pcb to a local IP address and port number.
		if (err == ERR_OK) { // If another connection is bound to the same port, the function will return ERR_USE, otherwise ERR_OK is returned.
			p->pcb = tcp_listen(p->pcb); // Commands pcb to start listening for incoming connections.
			// When an incoming connection is accepted, the function specified with the tcp_accept() function
			// will be called. pcb must have been bound to a local port with the tcp_bind() function.
			// The tcp_listen() function returns a new connection identifier, and the one passed as an
			// argument to the function will be deallocated. The reason for this behavior is that less
			// memory is needed for a connection that is listening, so tcp_listen() will reclaim the memory
			// needed for the original connection and allocate a new smaller memory block for the listening connection.
			// tcp_listen() may return NULL if no memory was available for the listening connection.
			// If so, the memory associated with pcb will not be deallocated.
			if (p->pcb != NULL) {
				tcp_arg(p->pcb, p->pcb);
				// insert new tcpsrv_config
				p->next = phcfg;
				phcfg = p;
				// initialize callback arg and accept callback
				tcp_accept(p->pcb, tcpsrv_server_accept);
#if DEBUGSOO > 0
				os_printf("Ok\n");
#endif
				return err;
			}
		}
		tcp_abandon(p->pcb, 0);
		p->pcb = NULL;
	} else
		err = ERR_MEM;
#if DEBUGSOO > 0
	os_printf("failed!\n");
#endif
	return err;
}
/******************************************************************************
 tcpsrv_close
 *******************************************************************************/
err_t ICACHE_FLASH_ATTR tcpsrv_close(TCP_SERV_CFG *p) {
	if (p == NULL) {
#if DEBUGSOO > 0
		os_printf("NULL pointer!\n");
#endif
		return ERR_ARG;
	};
#if DEBUGSOO > 0
	os_printf("\nTCP Service port %u closed - ", p->port);
#endif
	TCP_SERV_CFG **pwr = &phcfg;
	TCP_SERV_CFG *pcmp = phcfg;
	while (pcmp != NULL) {
		if (pcmp == p) {
			*pwr = p->next;
			TCP_SERV_CONN * ts_conn = p->conn_links;
			while (p->conn_links != NULL) {
				ts_conn->pcb = find_tcp_pcb(ts_conn);
				os_timer_disarm(&ts_conn->ptimer);
				if (ts_conn->pcb != NULL) {
					tcp_arg(ts_conn->pcb, NULL);
					tcp_recv(ts_conn->pcb, NULL);
					tcp_err(ts_conn->pcb, NULL);
					tcp_poll(ts_conn->pcb, NULL, 0);
					tcp_sent(ts_conn->pcb, NULL);
					tcp_abort(ts_conn->pcb);
				};
				tcpsrv_list_delete(ts_conn);
				ts_conn = p->conn_links;
			};
			if(p->pcb != NULL) tcp_close(p->pcb);
			os_free(p);
#if DEBUGSOO > 0
			os_printf("Ok\n");
#endif
			return ERR_OK; // break;
		}
		pwr = &pcmp->next;
		pcmp = pcmp->next;
	};
#if DEBUGSOO > 0
	os_printf("no find!\n");
#endif
	return ERR_CONN;
}
/******************************************************************************
 tcpsrv_close_port
 *******************************************************************************/
err_t ICACHE_FLASH_ATTR tcpsrv_close_port(uint16 portn)
{
	return tcpsrv_close(tcpsrv_port2pcfg(portn));
}
/******************************************************************************
 tcpsrv_close_all
 *******************************************************************************/
err_t ICACHE_FLASH_ATTR tcpsrv_close_all(void)
{
	err_t err = ERR_OK;
	while(phcfg != NULL && err == ERR_OK) err = tcpsrv_close(phcfg);
	return err;
}
/******************************************************************************
 tcpsrv_start_client
 TCP_SERV_CFG * p = tcpsrv_init(ID_CLIENTS_PORT);
			// insert new tcpsrv_config
			p->next = phcfg;
			phcfg = p;
 *******************************************************************************/
err_t ICACHE_FLASH_ATTR tcpsrv_client_start(TCP_SERV_CFG * p, uint32 remote_ip, uint16 remote_port) {
	err_t err = ERR_MEM;
	if (p == NULL) return err;
	if (system_get_free_heap_size() >= p->min_heap) {
		TCP_SERV_CONN * ts_conn = (TCP_SERV_CONN *) os_zalloc(sizeof(TCP_SERV_CONN));
		if (ts_conn != NULL) {
			ts_conn->flag = p->flag; // ��������� ����� �� ��������� �� ������ ����������
			ts_conn->pcfg = p;
			ts_conn->state = SRVCONN_CLIENT;
			ts_conn->remote_port = remote_port;
			ts_conn->remote_ip.dw = remote_ip;
			tcpsrv_client_connect(ts_conn);
			if(ts_conn->pcb != NULL) {
				// Insert new ts_conn
				ts_conn->next = p->conn_links;
				p->conn_links = ts_conn;
				p->conn_count++;
			} else {
#if DEBUGSOO > 0
				tcpsrv_print_remote_info(ts_conn);
				os_printf("tcp_connect - error %d\n", err);
#endif
				os_free(ts_conn);
			};
		}
		else {
#if DEBUGSOO > 0
			os_printf("srv[tcp_new] - out of mem!\n");
#endif
			err = ERR_MEM;
		};
	} else {
#if DEBUGSOO > 0
		os_printf("srv[new client] - low heap size!\n");
#endif
		err = ERR_MEM;
	};
	return err;
}
