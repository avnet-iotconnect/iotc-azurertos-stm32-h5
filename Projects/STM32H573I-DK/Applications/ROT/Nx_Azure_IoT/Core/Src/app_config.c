/**
  ******************************************************************************
  * @file    app_config.c
  * @author  GPM Application Team
  * @brief   Manage application configuration in secure storage.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the ST_LICENSE file
  * in the root directory of this software component.
  * If no ST_LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "app_config.h"
#include "app_azure_iot_config.h"
#include "string.h"
#include "stdio.h"
#include "psa/internal_trusted_storage.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
#define HEADER "AZURE10" /* This is a configuration for Azure. Rev 1.0 */

#define DEFAULT_SSID     "iot_demo"
#define DEFAULT_PSWD     "stm32h5"
#define DEFAULT_ENDPOINT "global.azure-devices-provisioning.net"
#define DEFAULT_IDSCOPE  "0ne00000000"

#define MAX_LENGTH_ENDPOINT        64
#define MAX_LENGTH_ID_SCOPE        16
#define MAX_LENGTH_HEADER          sizeof(HEADER)
#define MAX_LENGTH_SSID            32 /* MX_MAX_SSID_LEN */
#define MAX_LENGTH_KEY             64 /* MX_MAX_KEY_LEN */
#define CONFIG_UID               1

#define ADDRESS_HEADER        0
#define ADDRESS_ENDPOINT      (ADDRESS_HEADER   + MAX_LENGTH_HEADER  )
#define ADDRESS_IDSCOPE       (ADDRESS_ENDPOINT + MAX_LENGTH_ENDPOINT)
#define ADDRESS_SSID          (ADDRESS_IDSCOPE  + MAX_LENGTH_ID_SCOPE)
#define ADDRESS_PSWD          (ADDRESS_SSID     + MAX_LENGTH_SSID    )

#define MODIFY_ENDOINT  '0'
#define MODIFY_IDSCOPE  '1'
#define MODIFY_SSID     '2'
#define MODIFY_PASSWORD '3'
#define STORAGE_WRITE   '4'
#define STORAGE_READ    '5'
#define STORAGE_ERASE   '6'
#define SET_DEFAULT     '7'
#define DEVICE_RESET    '8'

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
char *pWIFI_SSID;
char *pWIFI_PASSWORD;
char *pENDPOINT;
char *pID_SCOPE;

static char config_buffer[ADDRESS_PSWD + MAX_LENGTH_KEY] = {0};
static char *header                            = &config_buffer[0];
static char *endpoint                          = &config_buffer[ADDRESS_ENDPOINT - ADDRESS_HEADER];
static char *idscope                           = &config_buffer[ADDRESS_IDSCOPE - ADDRESS_HEADER];
static char *ssid                              = &config_buffer[ADDRESS_SSID - ADDRESS_HEADER];
static char *pswd                              = &config_buffer[ADDRESS_PSWD - ADDRESS_HEADER];

/* Private function prototypes -----------------------------------------------*/
static uint32_t config_set_data(void);
static uint32_t config_erase_data(void);
static uint32_t config_set_default(void);

static void flushRN(void);

/**
  * @brief  Init the config interface and update the variables with the saved data.
  * @param  none
  * @retval status
  */
uint32_t config_init(void)
{
#if defined(__GNUC__)
  setvbuf(stdin, NULL, _IONBF, 0); /* no buffering to be able to input single character */
#endif /* if defined(__GNUC__) */
  return config_get_data();
}

/**
  * @brief  Display the menu
  * @param  none
  * @retval menu selection
  */
char config_display_menu(void)
{
  char choice = '\0';

  printf("\r\n%c - Modify Endpoint"              , MODIFY_ENDOINT );
  printf("\r\n%c - Modify ID Scope"              , MODIFY_IDSCOPE );
  printf("\r\n%c - Modify Wi-Fi SSID"            , MODIFY_SSID    );
  printf("\r\n%c - Modify Wi-Fi Password"        , MODIFY_PASSWORD);
  printf("\r\n%c - Write to STORAGE"             , STORAGE_WRITE  );
  printf("\r\n%c - Read parameters from STORAGE" , STORAGE_READ   );
  printf("\r\n%c - Erase parameters from STORAGE", STORAGE_ERASE  );
  printf("\r\n%c - Set Default values and reset" , SET_DEFAULT    );
  printf("\r\n%c - Reset the device"             , DEVICE_RESET   );
  printf("\r\n");

  choice = getchar();

  printf("\r\n");

  return choice;
}

/**
  * @brief  Process the commands
  * @param  none
  * @retval status
  */
uint32_t config_process_command(char command)
{
  switch(command)
  {
  case MODIFY_ENDOINT:
    printf("%s\r\n", "Enter Endpoint:");
    scanf("%s", endpoint);
    printf("Endpoint set to: %s\r\n", endpoint);
    flushRN();
    break;

  case MODIFY_IDSCOPE:
    printf("%s\r\n", "Enter ID Scope:");
    scanf("%s", idscope);
    printf("ID Scope set to: %s\r\n", idscope);
    flushRN();
    break;

  case MODIFY_SSID:
    printf("%s\r\n", "Enter Wi-Fi SSID:");
    scanf("%[^\r]s", ssid);
    printf("WiFi SSID set to: %s\r\n", ssid);
    flushRN();
    break;

  case MODIFY_PASSWORD:
    printf("%s\r\n", "Enter Wi-Fi PSWD:");
    scanf("%[^\r]s", pswd);
    printf("WiFi Password set to: %s\r\n", pswd);
    flushRN();
    break;

  case STORAGE_WRITE:
    sprintf(header, "%s", HEADER);
    config_set_data();
    break;

  case STORAGE_READ:
    config_get_data();
    break;

  case STORAGE_ERASE:
    config_erase_data();
    break;

  case SET_DEFAULT:
    config_set_default();
    break;

  case DEVICE_RESET:
    NVIC_SystemReset();
    break;

  default:
    printf("%s\r\n", "Error Re-Enter Number");
  }

  return CONFIG_SUCCESS;
}

/**
  * @brief  Read the saved config
  * @param  none
  * @retval status
  */
uint32_t config_get_data(void)
{
  psa_status_t psa_status = PSA_ERROR_GENERIC_ERROR;
  size_t actual_size = 0;

  psa_status = psa_its_get(CONFIG_UID, 0,
                   sizeof(config_buffer), config_buffer,
                   &actual_size);

  if (psa_status != PSA_SUCCESS)
  {
    printf("config_get_data psa_its_get failure (psa_status %ld)\r\n", psa_status);
    return CONFIG_ERROR;
  }

  if (strcmp(header, HEADER) != 0)
  {
    printf("header error\r\n");
    config_erase_data();

    return CONFIG_ERROR;
  }

  pWIFI_SSID     = ssid;
  pWIFI_PASSWORD = pswd;
  pENDPOINT      = endpoint;
  pID_SCOPE      = idscope;

  printf("\r\n");
  printf("HEADER   : %s\r\n", header);
  printf("SSID     : %s\r\n", pWIFI_SSID);
  printf("PSWD     : %s\r\n", pWIFI_PASSWORD);
  printf("ENDPOINT : %s\r\n", pENDPOINT);
  printf("ID SCOPE : %s\r\n", pID_SCOPE);
  printf("\r\n");

  return CONFIG_SUCCESS;
}

/**
  * @brief  Set configuration in secure storage
  * @param  none
  * @retval status
  */
static uint32_t config_set_data(void)
{
  psa_status_t psa_status = PSA_ERROR_GENERIC_ERROR;

  psa_status = psa_its_set(CONFIG_UID, sizeof(config_buffer),
                   config_buffer, 0);

  if (psa_status != PSA_SUCCESS)
  {
    printf("config_set_data psa_its_set failure (psa_status %ld)\r\n", psa_status);
    return CONFIG_ERROR;
  }

  return CONFIG_SUCCESS;
}

/**
  * @brief  Erase the config saved in secure storage
  * @param  none
  * @retval status
  */
static uint32_t config_erase_data(void)
{
  int32_t err = BSP_ERROR_NONE;

  memset(header  , 0, MAX_LENGTH_HEADER  );
  memset(endpoint, 0, MAX_LENGTH_ENDPOINT);
  memset(idscope , 0, MAX_LENGTH_ID_SCOPE);
  memset(ssid    , 0, MAX_LENGTH_SSID    );
  memset(pswd    , 0, MAX_LENGTH_KEY     );

  err = config_set_data();
  if (err != BSP_ERROR_NONE)
  {
    return CONFIG_ERROR;
  }


  return CONFIG_SUCCESS;
}

/**
  * @brief  Set the config to default.
  * @param  none
  * @retval status
  */
static uint32_t config_set_default(void)
{
  sprintf(header, "%s", HEADER);

  sprintf(ssid    , "%s", DEFAULT_SSID);
  pWIFI_SSID = ssid;

  sprintf(pswd    , "%s", DEFAULT_PSWD);
  pWIFI_PASSWORD = pswd;

  sprintf(endpoint, "%s", DEFAULT_ENDPOINT);
  pENDPOINT = endpoint;

  sprintf(idscope , "%s", DEFAULT_IDSCOPE);
  pID_SCOPE = idscope;

  config_process_command(STORAGE_WRITE);
  config_process_command(DEVICE_RESET);

  return CONFIG_SUCCESS;
}

/**
  * @brief
  * @param  file:
  * @retval none
  */
static void flushRN(void)
{
  char cur;

  do
  {
    scanf("%c", &cur);
  } while (cur != '\r');//\r -> CR || \n -> CR + LF (TeraTerm)
}
