/**************************************************************************/
/*                                                                        */
/*       Copyright (c) Microsoft Corporation. All rights reserved.        */
/*                                                                        */
/*       This software is licensed under the Microsoft Software License   */
/*       Terms for Microsoft Azure RTOS. Full text of the license can be  */
/*       found in the LICENSE file at https://aka.ms/AzureRTOS_EULA       */
/*       and in the root directory of this software.                      */
/*                                                                        */
/**************************************************************************/
/****************************************************************************************/
/*                                                                                      */
/* The following source code was taken from 'nx_azure_iot_adu_agent_ns_driver.c'        */
/* by Microsoft (https://github.com/azure-rtos) and modified by STMicroelectronics.     */
/* Main changes summary :                                                               */
/* - Common functions                                                                   */
/*                                                                                      */
/****************************************************************************************/

#include "nx_azure_iot_adu_agent_psa_driver.h"

/* common ADU driver for non-secure, secure and modules images.  */

static INT internal_flash_write(UCHAR *data_ptr, UINT data_size, UINT data_offset, nx_azure_iot_adu_agent_psa_driver_context_t* ctx);
static INT internal_version_compare(const UCHAR *buffer_ptr, UINT buffer_len, nx_azure_iot_adu_agent_psa_driver_context_t* ctx);

/****** DRIVER SPECIFIC ******/
void nx_azure_iot_adu_agent_psa_driver(NX_AZURE_IOT_ADU_AGENT_DRIVER *driver_req_ptr, nx_azure_iot_adu_agent_psa_driver_context_t* ctx)
{
psa_image_id_t dependency_uuid;
psa_image_version_t dependency_version;
psa_image_info_t info;
psa_status_t status;

    /* Default to successful return.  */
    driver_req_ptr -> nx_azure_iot_adu_agent_driver_status = NX_AZURE_IOT_SUCCESS;
        
    /* Process according to the driver request type.  */
    switch (driver_req_ptr -> nx_azure_iot_adu_agent_driver_command)
    {

        case NX_AZURE_IOT_ADU_AGENT_DRIVER_INITIALIZE:
        {

            /* Process initialize requests.  */
            break;
        }

        case NX_AZURE_IOT_ADU_AGENT_DRIVER_UPDATE_CHECK:
        {

            /* Read the version of image and compare the version in update_id to check if the update is installed or not.
               If installed, return NX_TRUE, else return NX_FALSE.  */
            if (internal_version_compare(driver_req_ptr -> nx_azure_iot_adu_agent_driver_installed_criteria, 
                                         driver_req_ptr -> nx_azure_iot_adu_agent_driver_installed_criteria_length,
                                         ctx) <= 0)
            {
                *(driver_req_ptr -> nx_azure_iot_adu_agent_driver_return_ptr) = NX_TRUE;
            }
            else
            {
                *(driver_req_ptr -> nx_azure_iot_adu_agent_driver_return_ptr) = NX_FALSE;
            }
            break;
        }

        case NX_AZURE_IOT_ADU_AGENT_DRIVER_PREPROCESS:
        {

            /* Process firmware preprocess requests before writing firmware.
               Such as: erase the flash at once to improve the speed.  */

            /* Abort the previous update if exists. */
            status = psa_fwu_abort(ctx->download_image_id);
            if((status == PSA_SUCCESS) || (status == PSA_ERROR_INVALID_ARGUMENT))
            {
                /*PSA_ERROR_INVALID_ARGUMENT can be returned when no image
                  with the provided image_id is currently being installed */
                ctx->firmware_size_total = driver_req_ptr -> nx_azure_iot_adu_agent_driver_firmware_size;
                ctx->firmware_size_count = 0;
                ctx->write_buffer_count = 0;

                if(_nx_utility_base64_decode((UCHAR*)driver_req_ptr->nx_azure_iot_adu_agent_driver_firmware_sha256,
                                           driver_req_ptr->nx_azure_iot_adu_agent_driver_firmware_sha256_length,
                                           ctx->sha256, sizeof(ctx->sha256), &(ctx->sha256_size)))
                {
                    driver_req_ptr -> nx_azure_iot_adu_agent_driver_status = NX_AZURE_IOT_FAILURE;
                }
            }
            else
            {
                driver_req_ptr -> nx_azure_iot_adu_agent_driver_status = NX_AZURE_IOT_FAILURE;
            }

            break;
        }

        case NX_AZURE_IOT_ADU_AGENT_DRIVER_WRITE:
        {

            /* Process firmware write requests.  */
            
            /* Write firmware contents.
               1. This function must be able to figure out which bank it should write to.
               2. Write firmware contents into new bank.
               3. Decrypt and authenticate the firmware itself if needed.
            */
            status = internal_flash_write(driver_req_ptr -> nx_azure_iot_adu_agent_driver_firmware_data_ptr,
                                          driver_req_ptr -> nx_azure_iot_adu_agent_driver_firmware_data_size,
                                          driver_req_ptr -> nx_azure_iot_adu_agent_driver_firmware_data_offset,
                                          ctx);
            if (status)
            {
                driver_req_ptr -> nx_azure_iot_adu_agent_driver_status = NX_AZURE_IOT_FAILURE;
            }

            break;
        }

        case NX_AZURE_IOT_ADU_AGENT_DRIVER_INSTALL:
        {

            if (ctx->firmware_size_count < ctx->firmware_size_total)
            {
                driver_req_ptr -> nx_azure_iot_adu_agent_driver_status = NX_AZURE_IOT_FAILURE;
                break;
            }
            
            /* Query the staging image. */
            status = psa_fwu_query(ctx->download_image_id, &info);
            if (status != PSA_SUCCESS)
            {
                driver_req_ptr -> nx_azure_iot_adu_agent_driver_status = NX_AZURE_IOT_FAILURE;
                break;
            }

            /* Check the image state. */
            if (info.state != PSA_IMAGE_CANDIDATE)
            {
                driver_req_ptr -> nx_azure_iot_adu_agent_driver_status = NX_AZURE_IOT_FAILURE;
            }

            /* Check the image signature. */
            if((PSA_FWU_MAX_DIGEST_SIZE != ctx->sha256_size) || (memcmp(info.digest, ctx->sha256, ctx->sha256_size) != 0))
            {              
                driver_req_ptr -> nx_azure_iot_adu_agent_driver_status = NX_AZURE_IOT_FAILURE;
                break;
            }

            /* Set the new firmware for next boot.  */
            status = psa_fwu_install(ctx->download_image_id, &dependency_uuid, &dependency_version);

            /* In the current implementation, image verification is deferred to
             * reboot, so PSA_SUCCESS_REBOOT is returned when success.
             */
            if ((status != PSA_SUCCESS_REBOOT) &&
                (status != PSA_ERROR_DEPENDENCY_NEEDED))
            {
                driver_req_ptr -> nx_azure_iot_adu_agent_driver_status = NX_AZURE_IOT_FAILURE;
                break;
            }

            /* Query the staging image. */
            status = psa_fwu_query(ctx->download_image_id, &info);
            if (status != PSA_SUCCESS)
            {
                driver_req_ptr -> nx_azure_iot_adu_agent_driver_status = NX_AZURE_IOT_FAILURE;
                break;
            }

            /* Check the image state. */
            if (info.state != PSA_IMAGE_REBOOT_NEEDED)
            {
                driver_req_ptr -> nx_azure_iot_adu_agent_driver_status = NX_AZURE_IOT_FAILURE;
            }

            break;
        }

        case NX_AZURE_IOT_ADU_AGENT_DRIVER_APPLY:
        {

            /* Apply the new firmware, and reboot device from that.*/
            psa_fwu_request_reboot();

            break;
        }
        default:
        {

            /* Invalid driver request.  */

            /* Default to successful return.  */
            driver_req_ptr -> nx_azure_iot_adu_agent_driver_status =  NX_AZURE_IOT_FAILURE;
        }
    }
}

static INT internal_version_compare(const UCHAR *buffer_ptr, UINT buffer_len, nx_azure_iot_adu_agent_psa_driver_context_t* ctx)
{
psa_image_info_t info;
psa_status_t status;
UINT i, j, pos;
UINT version[4] = {0};

    /* Get the version of the image.  */
    status = psa_fwu_query(ctx->active_image_id, &info);
    if (status != PSA_SUCCESS || info.state != PSA_IMAGE_INSTALLED)
    {
        return(-2);
    }

    for (i = 0, j = 0, pos = 0; i <= buffer_len; i++)
    {
        if ((buffer_ptr[i] == '.') || (i == buffer_len))
        {
            status = _nx_utility_string_to_uint((CHAR *)&buffer_ptr[pos], i - pos, &version[j]);
            if (status)
            {
                return(-2);
            }
            pos = i + 1;
            j++;
        }
    }

    /* Compare the version.  */
    if (version[0] > info.version.iv_major)
    {
        return(1);
    }
    if (version[0] < info.version.iv_major)
    {
        return(-1);
    }
    if (version[1] > info.version.iv_minor)
    {
        return(1);
    }
    if (version[1] < info.version.iv_minor)
    {
        return(-1);
    }
    if (version[2] > info.version.iv_revision)
    {
        return(1);
    }
    if (version[2] < info.version.iv_revision)
    {
        return(-1);
    }
    return 0;
}

static INT internal_flash_write(UCHAR *data_ptr, UINT data_size, UINT data_offset, nx_azure_iot_adu_agent_psa_driver_context_t* ctx)
{
psa_status_t status;
UINT remaining_size = data_size;
UINT block_size = 0;

    if (ctx->firmware_size_count + data_size + ctx->write_buffer_count > ctx->firmware_size_total)
    {
        return(NX_AZURE_IOT_FAILURE);
    }

    while ((ctx->write_buffer_count > 0) && (ctx->write_buffer_count < FLASH0_PROG_UNIT) && (remaining_size > 0))
    {
        ctx->write_buffer[ctx->write_buffer_count++] = *data_ptr++;
        data_offset++;
        remaining_size--;
    }

    if (ctx->write_buffer_count == FLASH0_PROG_UNIT)
    {
        status = psa_fwu_write(ctx->download_image_id, data_offset - FLASH0_PROG_UNIT, ctx->write_buffer, ctx->write_buffer_count);
        if (status != PSA_SUCCESS)
        {
            return(status);
        }
        ctx->firmware_size_count += ctx->write_buffer_count;
        ctx->write_buffer_count = 0;
    }

    while (remaining_size > FLASH0_PROG_UNIT)
    {
        if (remaining_size > PSA_FWU_MAX_BLOCK_SIZE)
        {
            block_size = PSA_FWU_MAX_BLOCK_SIZE;
        }
        else
        {
            block_size = remaining_size - (remaining_size % FLASH0_PROG_UNIT);
        }
        status = psa_fwu_write(ctx->download_image_id, data_offset, data_ptr, block_size);
        if (status != PSA_SUCCESS)
        {
            return(status);
        }
        remaining_size -= block_size;
        data_ptr += block_size;
        data_offset += block_size;
        ctx->firmware_size_count += block_size;
    }

    if (remaining_size > 0)
    {
        memcpy(&(ctx->write_buffer[ctx->write_buffer_count]), data_ptr, remaining_size);
        ctx->write_buffer_count += remaining_size;
    } else {
      // if last packet, this data_offset is used in the next psa_fwu_write, but it's not right as it's the end of the buffer, not the beginning.
      // if it's not the latest, it's not used and doesn't cause any problem
      data_offset -= ctx->write_buffer_count;
    }

    if ((ctx->write_buffer_count > 0) && (ctx->firmware_size_count + ctx->write_buffer_count >= ctx->firmware_size_total))
    {
        while (ctx->write_buffer_count < FLASH0_PROG_UNIT)
        {
            ctx->write_buffer[ctx->write_buffer_count++] = 0xFF;
        }
        //if data_offset not changed before, we will write at offset+write_buffer_count instead of offset
        status = psa_fwu_write(ctx->download_image_id, data_offset, ctx->write_buffer, ctx->write_buffer_count);
        if (status != PSA_SUCCESS)
        {
            return(status);
        }

        data_offset += ctx->write_buffer_count;
        ctx->firmware_size_count += ctx->write_buffer_count;
    }

    return(NX_SUCCESS);
}
