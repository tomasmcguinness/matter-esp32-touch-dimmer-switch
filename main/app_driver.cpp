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

#if CONFIG_ENABLE_CHIP_SHELL
static char console_buffer[101] = {0};
static esp_err_t app_driver_bound_console_handler(int argc, char **argv)
{
    if (argc == 1 && strncmp(argv[0], "help", sizeof("help")) == 0)
    {
        printf("Bound commands:\n"
               "\thelp: Print help\n"
               "\tinvoke: <local_endpoint_id> <cluster_id> <command_id> parameters ... \n"
               "\t\tExample: matter esp bound invoke 0x0001 0x0003 0x0000 0x78.\n");
    }
    else if (argc >= 4 && strncmp(argv[0], "invoke", sizeof("invoke")) == 0)
    {
        client::request_handle_t req_handle;
        req_handle.type = esp_matter::client::INVOKE_CMD;
        uint16_t local_endpoint_id = strtoul((const char *)&argv[1][2], NULL, 16);
        req_handle.command_path.mClusterId = strtoul((const char *)&argv[2][2], NULL, 16);
        req_handle.command_path.mCommandId = strtoul((const char *)&argv[3][2], NULL, 16);

        if (argc > 4)
        {
            console_buffer[0] = argc - 4;
            for (int i = 0; i < (argc - 4); i++)
            {
                if ((argv[4 + i][0] != '0') || (argv[4 + i][1] != 'x') ||
                    (strlen((const char *)&argv[4 + i][2]) > 10))
                {
                    ESP_LOGE(TAG, "Incorrect arguments. Check help for more details.");
                    return ESP_ERR_INVALID_ARG;
                }
                strcpy((console_buffer + 1 + 10 * i), &argv[4 + i][2]);
            }

            req_handle.request_data = console_buffer;
        }

        client::cluster_update(local_endpoint_id, &req_handle);
    }
    else
    {
        ESP_LOGE(TAG, "Incorrect arguments. Check help for more details.");
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

static esp_err_t app_driver_client_console_handler(int argc, char **argv)
{
    if (argc == 1 && strncmp(argv[0], "help", sizeof("help")) == 0)
    {
        printf("Client commands:\n"
               "\thelp: Print help\n"
               "\tinvoke: <fabric_index> <remote_node_id> <remote_endpoint_id> <cluster_id> <command_id> parameters "
               "... \n"
               "\t\tExample: matter esp client invoke 0x0001 0xBC5C01 0x0001 0x0003 0x0000 0x78.\n"
               "\tinvoke-group: <fabric_index> <group_id> <cluster_id> <command_id> parameters ... \n"
               "\t\tExample: matter esp client invoke-group 0x0001 0x257 0x0003 0x0000 0x78.\n");
    }
    else if (argc >= 6 && strncmp(argv[0], "invoke", sizeof("invoke")) == 0)
    {
        client::request_handle_t req_handle;
        req_handle.type = esp_matter::client::INVOKE_CMD;
        uint8_t fabric_index = strtoul((const char *)&argv[1][2], NULL, 16);
        uint64_t node_id = strtoull((const char *)&argv[2][2], NULL, 16);
        req_handle.command_path = {(chip::EndpointId)strtoul((const char *)&argv[3][2], NULL, 16) /* EndpointId */,
                                   0 /* GroupId */, strtoul((const char *)&argv[4][2], NULL, 16) /* ClusterId */,
                                   strtoul((const char *)&argv[5][2], NULL, 16) /* CommandId */,
                                   chip::app::CommandPathFlags::kEndpointIdValid};

        if (argc > 6)
        {
            console_buffer[0] = argc - 6;
            for (int i = 0; i < (argc - 6); i++)
            {
                if ((argv[6 + i][0] != '0') || (argv[6 + i][1] != 'x') ||
                    (strlen((const char *)&argv[6 + i][2]) > 10))
                {
                    ESP_LOGE(TAG, "Incorrect arguments. Check help for more details.");
                    return ESP_ERR_INVALID_ARG;
                }
                strcpy((console_buffer + 1 + 10 * i), &argv[6 + i][2]);
            }

            req_handle.request_data = console_buffer;
        }
        auto &server = chip::Server::GetInstance();
        client::connect(server.GetCASESessionManager(), fabric_index, node_id, &req_handle);
    }
    else if (argc >= 5 && strncmp(argv[0], "invoke-group", sizeof("invoke-group")) == 0)
    {
        client::request_handle_t req_handle;
        req_handle.type = esp_matter::client::INVOKE_CMD;
        uint8_t fabric_index = strtoul((const char *)&argv[1][2], NULL, 16);
        req_handle.command_path.mGroupId = strtoul((const char *)&argv[2][2], NULL, 16);
        req_handle.command_path.mClusterId = strtoul((const char *)&argv[3][2], NULL, 16);
        req_handle.command_path.mCommandId = strtoul((const char *)&argv[4][2], NULL, 16);
        req_handle.command_path = {
            0 /* EndpointId */, (chip::GroupId)strtoul((const char *)&argv[2][2], NULL, 16) /* GroupId */,
            strtoul((const char *)&argv[3][2], NULL, 16) /* ClusterId */,
            strtoul((const char *)&argv[4][2], NULL, 16) /* CommandId */, chip::app::CommandPathFlags::kGroupIdValid};

        if (argc > 5)
        {
            console_buffer[0] = argc - 5;
            for (int i = 0; i < (argc - 5); i++)
            {
                if ((argv[5 + i][0] != '0') || (argv[5 + i][1] != 'x') ||
                    (strlen((const char *)&argv[5 + i][2]) > 10))
                {
                    ESP_LOGE(TAG, "Incorrect arguments. Check help for more details.");
                    return ESP_ERR_INVALID_ARG;
                }
                strcpy((console_buffer + 1 + 10 * i), &argv[5 + i][2]);
            }

            req_handle.request_data = console_buffer;
        }

        client::group_request_send(fabric_index, &req_handle);
    }
    else
    {
        ESP_LOGE(TAG, "Incorrect arguments. Check help for more details.");
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

static void app_driver_register_commands()
{
    /* Add console command for bound devices */
    static const esp_matter::console::command_t bound_command = {
        .name = "bound",
        .description = "This can be used to simulate on-device control for bound devices."
                       "Usage: matter esp bound <bound_command>. "
                       "Bound commands: help, invoke",
        .handler = app_driver_bound_console_handler,
    };
    esp_matter::console::add_commands(&bound_command, 1);

    /* Add console command for client to control non-bound devices */
    static const esp_matter::console::command_t client_command = {
        .name = "client",
        .description = "This can be used to simulate on-device control for client devices."
                       "Usage: matter esp client <client_command>. "
                       "Client commands: help, invoke",
        .handler = app_driver_client_console_handler,
    };
    esp_matter::console::add_commands(&client_command, 1);
}
#endif // CONFIG_ENABLE_CHIP_SHELL

static void send_command_success_callback(void *context, const ConcreteCommandPath &command_path,
                                          const chip::app::StatusIB &status, TLVReader *response_data)
{
    ESP_LOGI(TAG, "Send command success");
}

static void send_command_failure_callback(void *context, CHIP_ERROR error)
{
    ESP_LOGI(TAG, "Send command failure: err :%" CHIP_ERROR_FORMAT, error.Format());
}

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
    // else if (req_handle->command_path.mClusterId == LevelControl::Id)
    // {
    //     strcpy(command_data_str, "{\"0:U8\": 128, \"1:U16\": 0, \"2:U8\": 0, \"3:U8\": 0}");
    // }
    else if (req_handle->command_path.mClusterId == LevelControl::Id)
    {
        strcpy(command_data_str, "{\"0:U8\": 0, \"1:U8\": 12, \"2:U16\": 0, \"3:U8\": 0, \"4:U8\": 0}");
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

static void app_driver_button_toggle_cb(void *arg, void *data)
{
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
        // TOOD Reverse the current dimming direction
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

app_driver_handle_t app_driver_switch_init()
{
    /* Initialize button */

    // button_event_args_t args = {
    //     .long_press.press_time = 2000,
    // };

    button_config_t config;
    memset(&config, 0, sizeof(button_config_t));

    config.type = BUTTON_TYPE_GPIO;
    config.gpio_button_config.gpio_num = GPIO_NUM_9;
    config.gpio_button_config.active_level = 0;

    button_handle_t handle = iot_button_create(&config);

    ESP_ERROR_CHECK(iot_button_register_cb(handle, BUTTON_PRESS_UP, app_driver_button_toggle_cb, NULL));
    ESP_ERROR_CHECK(iot_button_register_cb(handle, BUTTON_LONG_PRESS_HOLD, app_driver_button_dimming_cb, NULL));

    // To enable powersafe, a setting must be set via menuconfig
    //config.gpio_button_config.enable_power_save = true;

    /* Other initializations */
#if CONFIG_ENABLE_CHIP_SHELL
    app_driver_register_commands();
#endif // CONFIG_ENABLE_CHIP_SHELL
    client::set_request_callback(app_driver_client_invoke_command_callback,
                                 app_driver_client_group_invoke_command_callback, NULL);

    return (app_driver_handle_t)handle;
}
