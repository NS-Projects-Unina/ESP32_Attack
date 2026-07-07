#include "filesystem.h"

#include <stdio.h>
#include <sys/stat.h>

#include "esp_log.h"
#include "esp_spiffs.h"

static const char *TAG = "FILESYSTEM";

static bool mounted = false;

/*
 * SPIFFS Initialization
 */

esp_err_t filesystem_init(void)
{
    if (mounted)
    {
        return ESP_OK;
    }

    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .max_files = 10,
        .format_if_mount_failed = true
    };

    esp_err_t err = esp_vfs_spiffs_register(&conf);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG,
                 "Cannot mount SPIFFS (%s)",
                 esp_err_to_name(err));

        return err;
    }

    size_t total = 0;
    size_t used = 0;

    err = esp_spiffs_info("storage", &total, &used);

    if (err == ESP_OK)
    {
        ESP_LOGI(TAG,
                 "SPIFFS mounted");

        ESP_LOGI(TAG,
                 "Total: %u bytes",
                 (unsigned)total);

        ESP_LOGI(TAG,
                 "Used : %u bytes",
                 (unsigned)used);
    }

    mounted = true;

    return ESP_OK;
}

/*
 * SPIFFS Deinitialization
 */

esp_err_t filesystem_deinit(void)
{
    if (!mounted)
    {
        return ESP_OK;
    }

    esp_vfs_spiffs_unregister("storage");

    mounted = false;

    ESP_LOGI(TAG, "SPIFFS unmounted");

    return ESP_OK;
}

/*
 * File Exists
 */

bool filesystem_exists(const char *path)
{
    struct stat st;

    return (stat(path, &st) == 0);
}

/*
 * File Size
 */

size_t filesystem_file_size(const char *path)
{
    struct stat st;

    if (stat(path, &st) != 0)
    {
        return 0;
    }

    return st.st_size;
}

/*
 * Send File
 */

esp_err_t filesystem_send_file(httpd_req_t *req,
                               const char *path,
                               const char *content_type)
{
    FILE *file = fopen(path, "rb");

    if (file == NULL)
    {
        ESP_LOGE(TAG,
                 "File not found: %s",
                 path);

        httpd_resp_send_404(req);

        return ESP_FAIL;
    }

    httpd_resp_set_type(req, content_type);

    char buffer[1024];

    size_t read_bytes;

    while ((read_bytes = fread(buffer,
                               1,
                               sizeof(buffer),
                               file)) > 0)
    {
        if (httpd_resp_send_chunk(req,
                                  buffer,
                                  read_bytes) != ESP_OK)
        {
            fclose(file);

            httpd_resp_sendstr_chunk(req, NULL);

            return ESP_FAIL;
        }
    }

    fclose(file);

    httpd_resp_send_chunk(req, NULL, 0);

    return ESP_OK;
}