/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "esp_matter_client.h"
#include <cstddef>
#include <cstdio>
#include <esp_log.h>
#include <stdlib.h>
#include <string.h>

#include <esp_matter.h>
#include <esp_matter_console.h>

#include <iot_button.h>

#include <app_priv.h>
#include <app_reset.h>

#include <app/server/Server.h>
#include <lib/core/Optional.h>

using chip::kInvalidClusterId;
static constexpr chip::CommandId kInvalidCommandId = 0xFFFF'FFFF;

using namespace chip::app::Clusters;
using namespace esp_matter;
using namespace esp_matter::cluster;

static const char *TAG = "app_driver";
extern uint16_t switch_endpoint_id;

static void send_command_success_callback(void *context, const ConcreteCommandPath &command_path,
                                          const chip::app::StatusIB &status, TLVReader *response_data)
{
    ESP_LOGI(TAG, "Send command success");
}

static void send_command_failure_callback(void *context, CHIP_ERROR error)
{
    ESP_LOGI(TAG, "Send command failure: err :%" CHIP_ERROR_FORMAT, error.Format());
}

LevelControl::StepModeEnum current_step_direction = LevelControl::StepModeEnum::kUp;

void app_driver_client_invoke_command_callback(client::peer_device_t *peer_device, client::request_handle_t *req_handle,
                                               void *priv_data)
{
    if (req_handle->type != esp_matter::client::INVOKE_CMD)
    {
        return;
    }
    char command_data_str[256];
    // on_off light switch should support on_off cluster and identify cluster commands sending.
    if (req_handle->command_path.mClusterId == OnOff::Id)
    {
        strcpy(command_data_str, "{}");
    }
    else if (req_handle->command_path.mClusterId == LevelControl::Id)
    {
        sprintf(command_data_str, "{\"0:U8\": %d, \"1:U8\": 3, \"2:U16\": 0, \"3:U8\": 0, \"4:U8\": 0}", (int)current_step_direction);
    }
    else if (req_handle->command_path.mClusterId == Identify::Id)
    {
        if (req_handle->command_path.mCommandId == Identify::Commands::Identify::Id)
        {
            if (((char *)req_handle->request_data)[0] != 1)
            {
                ESP_LOGE(TAG, "Number of parameters error");
                return;
            }
            sprintf(command_data_str, "{\"0:U16\": %ld}",
                    strtoul((const char *)(req_handle->request_data) + 1, NULL, 16));
        }
        else
        {
            ESP_LOGE(TAG, "Unsupported command");
            return;
        }
    }
    else
    {
        ESP_LOGE(TAG, "Unsupported cluster: %ld", req_handle->command_path.mClusterId);
        return;
    }

    client::interaction::invoke::send_request(NULL,
                                              peer_device,
                                              req_handle->command_path,
                                              command_data_str,
                                              send_command_success_callback,
                                              send_command_failure_callback,
                                              chip::NullOptional);
}

void app_driver_client_group_invoke_command_callback(uint8_t fabric_index, client::request_handle_t *req_handle, void *priv_data)
{
    if (req_handle->type != esp_matter::client::INVOKE_CMD)
    {
        return;
    }
    char command_data_str[32];
    // on_off light switch should support on_off cluster and identify cluster commands sending.
    if (req_handle->command_path.mClusterId == OnOff::Id)
    {
        strcpy(command_data_str, "{}");
    }
    else if (req_handle->command_path.mClusterId == LevelControl::Id)
    {
        strcpy(command_data_str, "{}");
    }
    else if (req_handle->command_path.mClusterId == Identify::Id)
    {
        if (req_handle->command_path.mCommandId == Identify::Commands::Identify::Id)
        {
            if (((char *)req_handle->request_data)[0] != 1)
            {
                ESP_LOGE(TAG, "Number of parameters error");
                return;
            }
            sprintf(command_data_str, "{\"0:U16\": %ld}",
                    strtoul((const char *)(req_handle->request_data) + 1, NULL, 16));
        }
        else
        {
            ESP_LOGE(TAG, "Unsupported command");
            return;
        }
    }
    else
    {
        ESP_LOGE(TAG, "Unsupported cluster: %ld", req_handle->command_path.mClusterId);
        return;
    }

    client::interaction::invoke::send_group_request(fabric_index, req_handle->command_path, command_data_str);
}

static void swap_dimmer_direction() 
{
    // Swap the direction of the Step Command
    //
    if(current_step_direction == LevelControl::StepModeEnum::kUp) 
    {
        current_step_direction = LevelControl::StepModeEnum::kDown;
    }
    else 
    {
        current_step_direction = LevelControl::StepModeEnum::kUp;
    }

    ESP_LOGI(TAG, "Dimmer Direction is now: %d", (int)current_step_direction);
}

static void app_driver_button_press_up_cb(void *arg, void *data)
{
    ESP_LOGI(TAG, "Button Press Up");

    if (iot_button_get_ticks_time((button_handle_t)arg) < 1000)
    {
        ESP_LOGI(TAG, "Single Click");
        client::request_handle_t req_handle;
        req_handle.type = esp_matter::client::INVOKE_CMD;
        req_handle.command_path.mClusterId = OnOff::Id;
        req_handle.command_path.mCommandId = OnOff::Commands::Toggle::Id;

        lock::chip_stack_lock(portMAX_DELAY);
        client::cluster_update(switch_endpoint_id, &req_handle);
        lock::chip_stack_unlock();
    }
    else
    {
        ESP_LOGI(TAG, "Long Press");
        swap_dimmer_direction();
    }
}

static void app_driver_button_dimming_cb(void *arg, void *data)
{
    ESP_LOGI(TAG, "Long Press Hold");

    uint16_t hold_count = iot_button_get_long_press_hold_cnt((button_handle_t)arg);
    uint32_t hold_time = iot_button_get_ticks_time((button_handle_t)arg);

    ESP_LOGI(TAG, "Long Press Hold Count: %d", hold_count);
    ESP_LOGI(TAG, "Long Press Hold Time: %ld", hold_time);

    LevelControl::Commands::Step::Type stepCommand;
    stepCommand.stepMode = LevelControl::StepModeEnum::kUp;
    stepCommand.stepSize = 3;
    stepCommand.transitionTime.SetNonNull(0);
    stepCommand.optionsMask = static_cast<chip::BitMask<chip::app::Clusters::LevelControl::LevelControlOptions>>(0U);
    stepCommand.optionsOverride = static_cast<chip::BitMask<chip::app::Clusters::LevelControl::LevelControlOptions>>(0U);

    client::request_handle_t req_handle;
    req_handle.type = esp_matter::client::INVOKE_CMD;
    req_handle.command_path.mClusterId = LevelControl::Id;
    req_handle.command_path.mCommandId = LevelControl::Commands::Step::Id;
    req_handle.request_data = &stepCommand;

    lock::chip_stack_lock(portMAX_DELAY);
    client::cluster_update(switch_endpoint_id, &req_handle);
    lock::chip_stack_unlock();
}

static void app_driver_button_long_press_up_cb(void *arg, void *data)
{
    ESP_LOGI(TAG, "Long Press Up");
    swap_dimmer_direction();
}

static void app_driver_button_long_press_start_cb(void *arg, void *data)
{
    ESP_LOGI(TAG, "Long Press Started");
}

app_driver_handle_t app_driver_switch_init()
{
    button_config_t config;
    memset(&config, 0, sizeof(button_config_t));

    config.type = BUTTON_TYPE_GPIO;
    config.gpio_button_config.gpio_num = GPIO_NUM_9;
    config.gpio_button_config.active_level = 0;

    // To enable powersafe, a setting must be set first via menuconfig
    // config.gpio_button_config.enable_power_save = true;

    button_handle_t handle = iot_button_create(&config);

    ESP_ERROR_CHECK(iot_button_register_cb(handle, BUTTON_PRESS_UP, app_driver_button_press_up_cb, NULL));
    ESP_ERROR_CHECK(iot_button_register_cb(handle, BUTTON_LONG_PRESS_START, app_driver_button_long_press_start_cb, NULL));
    ESP_ERROR_CHECK(iot_button_register_cb(handle, BUTTON_LONG_PRESS_HOLD, app_driver_button_dimming_cb, NULL));

    // This event never seems to get raised.
    ESP_ERROR_CHECK(iot_button_register_cb(handle, BUTTON_LONG_PRESS_UP, app_driver_button_long_press_up_cb, NULL));

    /* Other initializations */
    client::set_request_callback(app_driver_client_invoke_command_callback,
                                 app_driver_client_group_invoke_command_callback, NULL);

    return (app_driver_handle_t)handle;
}
