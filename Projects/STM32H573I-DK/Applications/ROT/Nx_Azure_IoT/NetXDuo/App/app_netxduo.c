/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_netxduo.c
  * @author  GPM Application Team
  * @brief   NetXDuo applicative file
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2022 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the ST_LICENSE file
  * in the root directory of this software component.
  * If no ST_LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include <stdbool.h>
#include "app_netxduo.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "app_azure_rtos.h"
#include "nx_ip.h"
#ifndef USE_WIFI
#include "nx_stm32_eth_config.h"
#endif
#include "nxd_sntp_client.h"
#include "app_azure_iot.h"
#include "azrtos_time.h"
#include "iotconnect_app_config.h" // iotconnect app config for sntp time server value
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
TX_THREAD AppMainThread;
TX_THREAD AppAzureIotThread;

TX_SEMAPHORE DhcpSemaphore;

NX_PACKET_POOL        AppPool;
NX_IP                 IpInstance;
NX_DHCP               DhcpClient;
static NX_DNS         DnsClient;

#if 0
static NX_SNTP_CLIENT SntpClient;
#endif

ULONG   IpAddress;
ULONG   NetMask;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* Default time. GMT: Friday, Jan 1, 2022 12:00:00 AM. Epoch timestamp: 1640995200.  */
#ifndef SYSTEM_TIME 
#define SYSTEM_TIME              1640995200
#endif /* SYSTEM_TIME  */

/* EPOCH_TIME_DIFF is equivalent to 70 years in sec
   calculated with www.epochconverter.com/date-difference
   This constant is used to delete difference between :
   Unix time (referenced to 1970) and SNTP (referenced to 1900) */
#define EPOCH_TIME_DIFF          2208988800

#define SNTP_SYNC_MAX            (uint32_t)30
#define SNTP_UPDATE_MAX          (uint32_t)10
#define SNTP_UPDATE_INTERVAL     (NX_IP_PERIODIC_RATE / 2)

#define DHCP_TIMEOUT             30*NX_IP_PERIODIC_RATE

#ifdef USE_WIFI
#define NETXDUO_DRIVER nx_driver_emw3080_entry
#else
#define NETXDUO_DRIVER nx_stm32_eth_driver
#endif
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
extern      ULONG            UnixTime;

#if 0
static const char *sntp_servers[] =
{
  "0.pool.ntp.org",
  "1.pool.ntp.org",
  "2.pool.ntp.org",
  "3.pool.ntp.org"
};

static UINT sntp_server_index;
#endif

static ULONG dns_server_address[3];
static UINT  dns_server_address_size = sizeof(dns_server_address);
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
static VOID App_Main_Thread_Entry(ULONG thread_input);
static VOID App_Azure_IoT_Thread_Entry(ULONG thread_input);
static VOID ip_address_change_notify_callback(NX_IP *ip_instance, VOID *ptr);

#if 0
static UINT unix_time_get(ULONG *unix_time);

static UINT sntp_time_sync_internal(ULONG sntp_server_address);
static UINT sntp_time_sync(VOID);
#endif

extern TX_MUTEX ns_ipc_mutex;

/* USER CODE END PFP */

/**
  * @brief  Application NetXDuo Initialization.
  * @param memory_ptr: memory pointer
  * @retval int
  */
UINT MX_NetXDuo_Init(VOID *memory_ptr)
{
  UINT ret = NX_SUCCESS;
  TX_BYTE_POOL *byte_pool = (TX_BYTE_POOL*)memory_ptr;

   /* USER CODE BEGIN App_NetXDuo_MEM_POOL */

  /* USER CODE END App_NetXDuo_MEM_POOL */
  /* USER CODE BEGIN 0 */

  /* USER CODE END 0 */

  /* USER CODE BEGIN MX_NetXDuo_Init */
 
  ret = tx_mutex_create(&ns_ipc_mutex,"ns_ipc_mutex",
    TX_NO_INHERIT);
  if (ret != NX_SUCCESS)
  {
    printf("tx_mutex_create ns_ipc_mutex fail: %u\r\n", ret);
    return NX_NOT_ENABLED;
  }


#if (USE_STATIC_ALLOCATION == 1)
  printf("Start Azure IoT application...\r\n");

  CHAR *pointer;
  
  /* Allocate the memory for packet_pool.  */
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer,  NX_PACKET_POOL_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    printf("tx_byte_allocate (packet_pool) fail\r\n");
    return TX_POOL_ERROR;
  }
  
  /* Create the Packet pool to be used for packet allocation */
  ret = nx_packet_pool_create(&AppPool, "Main Packet Pool", PAYLOAD_SIZE, pointer, NX_PACKET_POOL_SIZE);
  
  if (ret != NX_SUCCESS)
  {
    printf("nx_packet_pool_create fail: %u\r\n", ret);
    return NX_NOT_ENABLED;
  }
  
  /* Allocate the memory for Ip_Instance */
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer, 2 * DEFAULT_MEMORY_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    printf("tx_byte_allocate (Ip_Instance) fail\r\n");
    return TX_POOL_ERROR;
  }

  printf("Create IP instance...\r\n");
  
  /* Create the main NX_IP instance */
  ret = nx_ip_create(&IpInstance, "Main Ip instance", NULL_ADDRESS, NULL_ADDRESS, &AppPool, NETXDUO_DRIVER,
                     pointer, 2 * DEFAULT_MEMORY_SIZE, DEFAULT_PRIORITY);
  
  if (ret != NX_SUCCESS)
  {
    printf("nx_ip_create fail: %u\r\n", ret);
    return NX_NOT_ENABLED;
  }
  
  /* create the DHCP client */
  ret = nx_dhcp_create(&DhcpClient, &IpInstance, "DHCP Client");
  
  if (ret != NX_SUCCESS)
  {
    printf("nx_dhcp_create fail: %u\r\n", ret);
    return NX_NOT_ENABLED;
  }
  
  /* Allocate the memory for ARP */
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer, ARP_MEMORY_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    printf("tx_byte_allocate (ARP) fail\r\n");
    return TX_POOL_ERROR;
  }
  
  /* Enable the ARP protocol and provide the ARP cache size for the IP instance */
  ret = nx_arp_enable(&IpInstance, (VOID *)pointer, ARP_MEMORY_SIZE);
  
  if (ret != NX_SUCCESS)
  {
    printf("nx_arp_enable fail: %u\r\n", ret);
    return NX_NOT_ENABLED;
  }
  
  /* Enable the ICMP */
  ret = nx_icmp_enable(&IpInstance);
  
  if (ret != NX_SUCCESS)
  {
    printf("nx_icmp_enable fail: %u\r\n", ret);
    return NX_NOT_ENABLED;
  }
  
  /* Enable the UDP protocol required for DHCP communication */
  ret = nx_udp_enable(&IpInstance);
  
  if (ret != NX_SUCCESS)
  {
    printf("nx_udp_enable fail: %u\r\n", ret);
    return NX_NOT_ENABLED;
  }
  
  /* Enable the TCP protocol required for MQTT, ... */
  ret = nx_tcp_enable(&IpInstance);
  
  if (ret != NX_SUCCESS)
  {
    printf("nx_tcp_enable fail: %u\r\n", ret);
    return NX_NOT_ENABLED;
  }
  
  /* Allocate the memory for main thread   */
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer, THREAD_MEMORY_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    printf("tx_byte_allocate (main thread) fail\r\n");
    return TX_POOL_ERROR;
  }
  
  /* Initialize TLS. */
  nx_secure_tls_initialize();
  
  /* Create the main thread */
  ret = tx_thread_create(&AppMainThread, "App Main thread", App_Main_Thread_Entry, 0, pointer, THREAD_MEMORY_SIZE,
                         DEFAULT_MAIN_PRIORITY, DEFAULT_MAIN_PRIORITY, TX_NO_TIME_SLICE, TX_AUTO_START);
  
  if (ret != TX_SUCCESS)
  {
    printf("tx_thread_create (App Main thread) fail: %u\r\n", ret);
    return NX_NOT_ENABLED;
  }
  
  /* Allocate the memory for Azure IoT application thread   */
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer, 2 * THREAD_MEMORY_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    printf("tx_byte_allocate (Azure IoT application thread) fail\r\n");
    return TX_POOL_ERROR;
  }
  
  /* create the Azure IoT application thread */
  ret = tx_thread_create(&AppAzureIotThread, "Azure IoT App", App_Azure_IoT_Thread_Entry, 0, pointer, THREAD_MEMORY_SIZE,
                         APP_PRIORITY, APP_PRIORITY, TX_NO_TIME_SLICE, TX_DONT_START);
  
  if (ret != TX_SUCCESS)
  {
    printf("tx_thread_create (Azure IoT Thread) fail: %u\r\n", ret);
    return NX_NOT_ENABLED;
  }
  
  /* set DHCP notification callback  */
  tx_semaphore_create(&DhcpSemaphore, "DHCP Semaphore", 0);
#endif 
  /* USER CODE END MX_NetXDuo_Init */

  return ret;
}

/* USER CODE BEGIN 1 */
/**
* @brief  ip address change callback.
* @param ip_instance: NX_IP instance
* @param ptr: user data
* @retval none
*/
static VOID ip_address_change_notify_callback(NX_IP *ip_instance, VOID *ptr)
{
  /* release the semaphore as soon as an IP address is available */
  tx_semaphore_put(&DhcpSemaphore);
}

/**
* @brief  Main thread entry.
* @param thread_input: ULONG user argument used by the thread entry
* @retval none
*/
static VOID App_Main_Thread_Entry(ULONG thread_input)
{
  UINT ret = NX_SUCCESS;
  
  printf("Get IP Address...\r\n");

  ret = nx_ip_address_change_notify(&IpInstance, ip_address_change_notify_callback, NULL);
  if (ret != NX_SUCCESS)
  {
    printf("nx_ip_address_change_notify fail: %u\r\n", ret);
    Error_Handler();
  }

#ifndef USE_WIFI
  ULONG link_status;
  do
  {
    /* Get Ethernet Physical Link status. */
    ret = nx_ip_interface_status_check(&IpInstance, 0, NX_IP_LINK_ENABLED,
                                      &link_status, 10);
    if (ret != NX_SUCCESS)
    {
      printf("Ethernet interface not ready. Please check the cable.\r\n");
      tx_thread_sleep(3*TX_TIMER_TICKS_PER_SECOND);
    }
    else
    {
      nx_ip_driver_direct_command(&IpInstance, NX_LINK_ENABLE,
                            &link_status);
    }
  } while (ret != NX_SUCCESS);
#endif /* ifndef USE_WIFI */

  /* start DHCP client */
  ret = nx_dhcp_start(&DhcpClient);
  if (ret != NX_SUCCESS)
  {
    printf("nx_dhcp_start fail: %u\r\n", ret);
    Error_Handler();
  }
  
  /* wait until an IP address is ready */
  if(tx_semaphore_get(&DhcpSemaphore, DHCP_TIMEOUT) != TX_SUCCESS)
  {
    printf("nx_dhcp timeout fail\r\n");
    Error_Handler();
  }
  
  ret = nx_ip_address_get(&IpInstance, &IpAddress, &NetMask);
  
  if (ret != TX_SUCCESS)
  {
    printf("nx_ip_address_get fail: %u\r\n", ret);
    Error_Handler();
  }
  
  PRINT_IP_ADDRESS("STM32 IP Address: ", IpAddress);

#ifndef USER_DNS_ADDRESS
  /* Retrieve DNS server address from DHCP answer */
  nx_dhcp_interface_user_option_retrieve(&DhcpClient, 0, NX_DHCP_OPTION_DNS_SVR, (UCHAR *)(dns_server_address),
                                           &dns_server_address_size);
#endif

  /* start the Azure IoT application thread */
  tx_thread_resume(&AppAzureIotThread);
  
  /* this thread is not needed any more, we relinquish it */
  tx_thread_relinquish();
}

/**
* @brief  DNS Create Function.
* @param dns_ptr
* @retval ret
*/
UINT dns_create(NX_DNS *dns_ptr)
{
  UINT ret = NX_SUCCESS;
  
  /* Create a DNS instance for the Client */
  ret = nx_dns_create(dns_ptr, &IpInstance, (UCHAR *)"DNS Client");
  if (ret)
  {
    printf("nx_dns_create fail: %u\r\n", ret);
    Error_Handler();
  }

#ifdef USER_DNS_ADDRESS
  dns_server_address[0] = USER_DNS_ADDRESS;
#endif

  /* Initialize DNS instance with a DNS server address */
  ret = nx_dns_server_add(dns_ptr, dns_server_address[0]);
  if (ret)
  {
    printf("nx_dns_server_add fail: %u\r\n", ret);
    Error_Handler();
  }
  PRINT_IP_ADDRESS("DNS Server address:", dns_server_address[0]);
  return ret;
}


/**
* @brief  Azure IoT application thread entry.
* @param  thread_input: ULONG user argument used by the thread entry
* @retval none
*/
static VOID App_Azure_IoT_Thread_Entry(ULONG thread_input)
{
  UINT ret = NX_SUCCESS;
  
  /* Create a DNS client */
  ret = dns_create(&DnsClient);
  
  if (ret != NX_SUCCESS)
  {
    printf("dns_create fail: %u\r\n", ret);
    Error_Handler();
  }
  
  /* Sync up time by SNTP at start up. */
  //  ret = sntp_time_sync();
  ret = sntp_time_sync(&IpInstance, &AppPool, &DnsClient, SAMPLE_SNTP_SERVER_NAME);

  /* Check status.  */
  if (ret != NX_SUCCESS)
  {
    printf("SNTP Time Sync failed.\r\n");
    Error_Handler();
  }

  /* run Azure IoT application code */
  //app_azure_iot_entry(&IpInstance, &AppPool, &DnsClient, unix_time_get);
  extern bool app_startup(NX_IP *ip_ptr, NX_PACKET_POOL *pool_ptr, NX_DNS *dns_ptr);

  app_startup(&IpInstance, &AppPool, &DnsClient);
}
#if 0
UINT unix_time_get(ULONG *unix_time)
{
  /* Return number of seconds since Unix Epoch (1/1/1970 00:00:00).  */
  *unix_time =  UnixTime;

  return(NX_SUCCESS);
}



/* Sync up the local time with a known SNTP server. */
static UINT sntp_time_sync_internal(ULONG sntp_server_address)
{
  UINT ret;

  /* Create the SNTP Client to run in broadcast mode.. */
  ret = nx_sntp_client_create(&SntpClient, &IpInstance, 0, &AppPool,
                              NX_NULL,
                              NX_NULL,
                              NX_NULL /* no random_number_generator callback */);

  /* Check status.  */
  if (ret != NX_SUCCESS)
  {
    printf("nx_sntp_client_create fail: %u\r\n", ret);
    return ret;
  }

  printf("Trying SNTP server : %lu.%lu.%lu.%lu\r\n", (sntp_server_address>>24) & 0xFF,
                                             (sntp_server_address>>16) & 0xFF,
                                             (sntp_server_address>>8) & 0xFF,
                                             sntp_server_address & 0xFF);
  
  /* Use the IPv4 service to initialize the Client and set the IPv4 SNTP server. */
  ret = nx_sntp_client_initialize_unicast(&SntpClient, sntp_server_address);

  /* Check status.  */
  if (ret != NX_SUCCESS)
  {
    printf("nx_sntp_client_initialize_unicast fail: %u\r\n", ret);
    nx_sntp_client_delete(&SntpClient);
    return ret;
  }

  /* Set local time to 0 */
  ret = nx_sntp_client_set_local_time(&SntpClient, 0, 0);

  /* Check status.  */
  if (ret != NX_SUCCESS)
  {
    printf("nx_sntp_client_set_local_time fail: %u\r\n", ret);
    nx_sntp_client_delete(&SntpClient);
    return ret;
  }

  /* Run Unicast client */
  ret = nx_sntp_client_run_unicast(&SntpClient);

  /* Check status.  */
  if (ret != NX_SUCCESS)
  {
    printf("nx_sntp_client_run_unicast fail: %u\r\n", ret);
    nx_sntp_client_stop(&SntpClient);
    nx_sntp_client_delete(&SntpClient);
    return ret;
  }

  /* Wait till updates are received */
  for (uint32_t i = 0; i < SNTP_UPDATE_MAX; i++)
  {
    UINT server_status;

    /* First verify we have a valid SNTP service running. */
    ret = nx_sntp_client_receiving_updates(&SntpClient, &server_status);

    /* Check status.  */
    if ((ret == NX_SUCCESS) && (server_status == NX_TRUE))
    {
      /* Server status is good. Now get the Client local time. */
      ULONG sntp_seconds;
      ULONG sntp_fraction;

      /* Get the local time. */
      ret = nx_sntp_client_get_local_time_extended(&SntpClient,
                                                   &sntp_seconds, &sntp_fraction,
                                                   NULL, 0);

      /* Check status. */
      if (ret != NX_SUCCESS)
      {
        continue;
      }

      /* Convert NTP time (01/01/1900 0:0:0) to Unix time (01/01/1970 0:0:0) */
      UnixTime = sntp_seconds - EPOCH_TIME_DIFF;

      /* Stop and delete SNTP. */
      nx_sntp_client_stop(&SntpClient);
      nx_sntp_client_delete(&SntpClient);

      return NX_SUCCESS;
    }

    /* Sleep.  */
    tx_thread_sleep(SNTP_UPDATE_INTERVAL);
  }

  /* Time sync failed.  */

  /* Stop and delete SNTP.  */
  nx_sntp_client_stop(&SntpClient);
  nx_sntp_client_delete(&SntpClient);

  /* Return success.  */
  return NX_NOT_SUCCESSFUL;
}

/* walk through the list of SNTP servers to find one to synchronize time */
static UINT sntp_time_sync(VOID)
{
  UINT status;
  ULONG sntp_server_address[3];
#ifndef DHCP_DISABLE
  UINT  sntp_server_address_size = sizeof(sntp_server_address);
#endif

#ifndef DHCP_DISABLE
    /* if DHCP server returned an NTP server address then try to sync the time with it */
    status = nx_dhcp_interface_user_option_retrieve(&DhcpClient, 0, NX_DHCP_OPTION_NTP_SVR, (UCHAR *)(sntp_server_address),
                                                    &sntp_server_address_size);

    /* Check status.  */
    if (status == NX_SUCCESS)
    {
        for (UINT i = 0; (i * 4) < sntp_server_address_size; i++)
        {
          printf("SNTP Time Sync... %lu.%lu.%lu.%lu (from DHCP)\r\n", 
                   (sntp_server_address[i] >> 24),
                   (sntp_server_address[i] >> 16 & 0xFF),
                   (sntp_server_address[i] >> 8 & 0xFF),
                   (sntp_server_address[i] & 0xFF));

            /* Start SNTP to sync the local time.  */
            status = sntp_time_sync_internal(sntp_server_address[i]);

            /* Check status.  */
            if(status == NX_SUCCESS)
            {
                return(NX_SUCCESS);
            }
        }
    }
#endif /* DHCP_DISABLE */

  /* If no NTP server address obtained with DHCP then try with official list */
  for (uint32_t i = 0; i < SNTP_SYNC_MAX; i++)
  {
    printf("SNTP Time Sync... %s\r\n", sntp_servers[sntp_server_index]);

    /* Look up SNTP Server address. */
    status = nx_dns_host_by_name_get(&DnsClient, (UCHAR *)sntp_servers[sntp_server_index], &sntp_server_address[0],
                                     5 * NX_IP_PERIODIC_RATE);

    /* Check status.  */
    if (status == NX_SUCCESS)
    {
      /* Start SNTP to sync the local time. */
      status = sntp_time_sync_internal(sntp_server_address[0]);

      /* Check status.  */
      if (status == NX_SUCCESS)
      {
        return NX_SUCCESS;
      }
    }

    /* Switch SNTP server every time. */
    sntp_server_index = (sntp_server_index + 1) % (sizeof(sntp_servers) / sizeof(sntp_servers[0]));
  }

  return NX_NOT_SUCCESSFUL;
}

#endif // if 0

/* USER CODE END 1 */
