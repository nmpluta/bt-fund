/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/addr.h>
#include <zephyr/bluetooth/conn.h>
#include <bluetooth/services/lbs.h>

#include <dk_buttons_and_leds.h>

#define USER_BUTTON            DK_BTN1_MSK
#define RUN_STATUS_LED         DK_LED1
#define CONNECTION_STATUS_LED  DK_LED2
#define RUN_LED_BLINK_INTERVAL 1000

struct bt_conn *my_conn = NULL;
static struct k_work adv_work;

static const struct bt_le_adv_param *adv_param = BT_LE_ADV_PARAM(
	(BT_LE_ADV_OPT_CONN |
	 BT_LE_ADV_OPT_USE_IDENTITY), /* Connectable advertising and use identity address */
	BT_GAP_ADV_FAST_INT_MIN_1,    /* 0x30 units, 48 units, 30ms */
	BT_GAP_ADV_FAST_INT_MAX_1,    /* 0x60 units, 96 units, 60ms */
	NULL);                        /* Set to NULL for undirected advertising */

LOG_MODULE_REGISTER(Lesson3_Exercise2, LOG_LEVEL_INF);

/* STEP 11.2 - Create variable that holds callback for MTU negotiation */
static struct bt_gatt_exchange_params exchange_params;

/* STEP 13.4 - Forward declaration of exchange_func(): */
static void exchange_func(struct bt_conn *conn, uint8_t att_err,
			  struct bt_gatt_exchange_params *params);

#define DEVICE_NAME     CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static const struct bt_data sd[] = {
	BT_DATA_BYTES(BT_DATA_UUID128_ALL,
		      BT_UUID_128_ENCODE(0x00001523, 0x1212, 0xefde, 0x1523, 0x785feabcd123)),
};

static void adv_work_handler(struct k_work *work)
{
	int err = bt_le_adv_start(adv_param, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));

	if (err) {
		LOG_ERR("Advertising failed to start (err %d)", err);
		return;
	}

	LOG_INF("Advertising successfully started");
}

static void advertising_start(void)
{
	k_work_submit(&adv_work);
}

/* STEP 7.1 - Define the function to update the connection's PHY */
static void update_phy(struct bt_conn *conn)
{
	int err;

	/* Request to use the 2M PHY for both TX and RX */
	const struct bt_conn_le_phy_param phy_param = {
		.options = BT_CONN_LE_PHY_OPT_NONE,
		.pref_rx_phy = BT_GAP_LE_PHY_2M,
		.pref_tx_phy = BT_GAP_LE_PHY_2M,
	};

	err = bt_conn_le_phy_update(conn, &phy_param);
	if (err) {
		LOG_ERR("Failed to initiate PHY update (err %d)", err);
	}
}

/* STEP 10 - Define the function to update the connection's data length */
static void update_data_length(struct bt_conn *conn)
{
	int err;

	/* Request to use maximum data length */
	const struct bt_conn_le_data_len_param data_len_param = {
		.tx_max_len = BT_GAP_DATA_LEN_MAX,
		.tx_max_time = BT_GAP_DATA_TIME_MAX,
	};

	err = bt_conn_le_data_len_update(conn, &data_len_param);
	if (err) {
		LOG_ERR("Failed to initiate Data Length update (err %d)", err);
	}
}

/* STEP 11.1 - Define the function to update the connection's MTU */
static void update_mtu(struct bt_conn *conn)
{
	int err;
	exchange_params.func = exchange_func;

	/* Request to exchange MTU */
	err = bt_gatt_exchange_mtu(conn, &exchange_params);
	if (err) {
		LOG_ERR("Failed to initiate MTU exchange (err %d)", err);
	}
}

/* Callbacks */
void on_connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		LOG_ERR("Connection error %d", err);
		return;
	}
	LOG_INF("Connected");
	my_conn = bt_conn_ref(conn);
	dk_set_led(CONNECTION_STATUS_LED, 1);
	k_sleep(K_MSEC(100));

	/* STEP 1.1 - Declare a structure to store the connection parameters */
	struct bt_conn_info info;

	/* STEP 1.2 - Add the connection parameters to your log */
	err = bt_conn_get_info(my_conn, &info);
	if (err) {
		LOG_ERR("Failed to get connection info (err %d)", err);
		return;
	}
	LOG_INF("Connection parameters:");
	LOG_INF("  Interval: %.2f ms", (double)info.le.interval * 1.25);
	LOG_INF("  Latency: %d", info.le.latency);
	LOG_INF("  Supervision timeout: %d ms", info.le.timeout * 10);

	/* STEP 7.2 - Update the PHY mode */
	LOG_INF("Updating PHY...");
	update_phy(my_conn);

	/* Add delays to avoid a link layer collision */
	k_sleep(K_MSEC(1000));

	/* STEP 13.5 - Update the data length and MTU */
	LOG_INF("Updating Data Length...");
	update_data_length(my_conn);
	LOG_INF("Updating MTU...");
	update_mtu(my_conn);
}

void on_disconnected(struct bt_conn *conn, uint8_t reason)
{
	LOG_INF("Disconnected. Reason %d", reason);
	dk_set_led(CONNECTION_STATUS_LED, 0);
	bt_conn_unref(my_conn);
}

void on_recycled(void)
{
	advertising_start();
}

/* STEP 4.2 - Add the callback for connection parameter updates */
void on_le_param_updated(struct bt_conn *conn, uint16_t interval, uint16_t latency,
			 uint16_t timeout)
{
	LOG_INF("Connection parameters updated:");
	LOG_INF("  Interval: %.2f ms", (double)interval * 1.25);
	LOG_INF("  Latency: %d", latency);
	LOG_INF("  Supervision timeout: %d ms", timeout * 10);
}

const char *bt_phy_str(uint8_t phy)
{
	switch (phy) {
	case BT_CONN_LE_TX_POWER_PHY_NONE:
		return "No PHY set";
	case BT_CONN_LE_TX_POWER_PHY_1M:
		return "LE 1M PHY";
	case BT_CONN_LE_TX_POWER_PHY_2M:
		return "LE 2M PHY";
	case BT_CONN_LE_TX_POWER_PHY_CODED_S8:
		return "LE Coded PHY S=8";
	case BT_CONN_LE_TX_POWER_PHY_CODED_S2:
		return "LE Coded PHY S=2";
	default:
		return "Unknown PHY";
	}
}

/* STEP 8.1 - Write a callback function to inform about updates in the PHY */
void on_le_phy_updated(struct bt_conn *conn, struct bt_conn_le_phy_info *param)
{
	LOG_INF("PHY updated:");
	LOG_INF("  TX PHY: %s", bt_phy_str(param->tx_phy));
	LOG_INF("  RX PHY: %s", bt_phy_str(param->rx_phy));
}

/* STEP 13.1 - Write a callback function to inform about updates in data length */
void on_le_data_len_updated(struct bt_conn *conn, struct bt_conn_le_data_len_info *info)
{
	LOG_INF("Data Length updated:");
	LOG_INF("  TX Max Len: %d", info->tx_max_len);
	LOG_INF("  TX Max Time: %d us", info->tx_max_time);
	LOG_INF("  RX Max Len: %d", info->rx_max_len);
	LOG_INF("  RX Max Time: %d us", info->rx_max_time);
}

struct bt_conn_cb connection_callbacks = {
	.connected = on_connected,
	.disconnected = on_disconnected,
	.recycled = on_recycled,
	/* STEP 4.1 - Add the callback for connection parameter updates */
	.le_param_updated = on_le_param_updated,
	/* STEP 8.3 - Add the callback for PHY mode updates */
	.le_phy_updated = on_le_phy_updated,
	/* STEP 13.2 - Add the callback for data length updates */
	.le_data_len_updated = on_le_data_len_updated,
};

/* STEP 13.3 - Implement callback function for MTU exchange */
static void exchange_func(struct bt_conn *conn, uint8_t att_err,
			  struct bt_gatt_exchange_params *params)
{
	LOG_INF("MTU exchange completed with %s", att_err ? "error" : "success");
	if (att_err) {
		LOG_ERR("ATT error code: %d", att_err);
		return;
	}
	uint16_t payload_mtu = bt_gatt_get_mtu(conn) - 3; /* Subtract 3 bytes for ATT header */
	LOG_INF("Negotiated MTU: %d bytes", payload_mtu);
}

static void button_changed(uint32_t button_state, uint32_t has_changed)
{
	int err;
	bool user_button_changed = (has_changed & USER_BUTTON) ? true : false;
	bool user_button_pressed = (button_state & USER_BUTTON) ? true : false;
	if (user_button_changed) {
		LOG_INF("Button %s", (user_button_pressed ? "pressed" : "released"));

		err = bt_lbs_send_button_state(user_button_pressed);
		if (err) {
			LOG_ERR("Couldn't send notification. (err: %d)", err);
		}
	}
}

static int init_button(void)
{
	int err;

	err = dk_buttons_init(button_changed);
	if (err) {
		LOG_ERR("Cannot init buttons (err: %d)", err);
	}

	return err;
}

int main(void)
{
	int blink_status = 0;
	int err;

	LOG_INF("Starting Lesson 3 - Exercise 2\n");

	err = dk_leds_init();
	if (err) {
		LOG_ERR("LEDs init failed (err %d)", err);
		return -1;
	}

	err = init_button();
	if (err) {
		LOG_ERR("Button init failed (err %d)", err);
		return -1;
	}

	err = bt_conn_cb_register(&connection_callbacks);
	if (err) {
		LOG_ERR("Connection callback register failed (err %d)", err);
	}

	err = bt_enable(NULL);
	if (err) {
		LOG_ERR("Bluetooth init failed (err %d)", err);
		return -1;
	}

	LOG_INF("Bluetooth initialized");
	k_work_init(&adv_work, adv_work_handler);
	advertising_start();

	for (;;) {
		dk_set_led(RUN_STATUS_LED, (++blink_status) % 2);
		k_sleep(K_MSEC(RUN_LED_BLINK_INTERVAL));
	}
}
