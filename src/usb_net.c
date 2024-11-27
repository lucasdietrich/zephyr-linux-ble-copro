/*
 * Copyright (c) 2024 Lucas Dietrich <ld.adecy@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <usb_net.h>

#include <zephyr/logging/log.h>
#include <zephyr/net/conn_mgr_monitor.h>
#include <zephyr/net/ethernet_mgmt.h>
#include <zephyr/net/net_context.h>
#include <zephyr/net/net_core.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/net_mgmt.h>

LOG_MODULE_REGISTER(usb_net_mgmt, LOG_LEVEL_INF);

#define MGMT_ETHERNET_CB_INDEX	0u
#define MGMT_INTERFACE_CB_INDEX 1u
#define MGMT_IP_CB_INDEX		2u
#define MGMT_L4_CB_INDEX		3u
static struct net_mgmt_event_callback mgmt_cb[4u];

static void net_event_handler(struct net_mgmt_event_callback *cb,
							  uint32_t mgmt_event,
							  struct net_if *iface);
static void usb_iface_configure(struct net_if *iface);
static void usb_iface_deinit(struct net_if *iface);

void usb_net_iface_init(void)
{
	/* One callback per layer */
	net_mgmt_init_event_callback(&mgmt_cb[MGMT_ETHERNET_CB_INDEX], net_event_handler,
								 NET_EVENT_ETHERNET_CARRIER_ON |
									 NET_EVENT_ETHERNET_CARRIER_OFF);
	net_mgmt_add_event_callback(&mgmt_cb[MGMT_ETHERNET_CB_INDEX]);

	net_mgmt_init_event_callback(&mgmt_cb[MGMT_INTERFACE_CB_INDEX], net_event_handler,
								 NET_EVENT_IF_UP | NET_EVENT_IF_DOWN);
	net_mgmt_add_event_callback(&mgmt_cb[MGMT_INTERFACE_CB_INDEX]);

	net_mgmt_init_event_callback(
		&mgmt_cb[MGMT_IP_CB_INDEX], net_event_handler,
		NET_EVENT_IPV4_ADDR_ADD | NET_EVENT_IPV4_ADDR_DEL | NET_EVENT_IPV4_DHCP_START |
			NET_EVENT_IPV4_DHCP_BOUND | NET_EVENT_IPV4_DHCP_STOP);
	net_mgmt_add_event_callback(&mgmt_cb[MGMT_IP_CB_INDEX]);

	net_mgmt_init_event_callback(&mgmt_cb[MGMT_L4_CB_INDEX], net_event_handler,
								 NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED);
	net_mgmt_add_event_callback(&mgmt_cb[MGMT_L4_CB_INDEX]);

	conn_mgr_mon_resend_status();
}

#define _CR(_val)                                                                        \
	case _val:                                                                           \
		return #_val

static const char *net_mgmt_event_to_str(uint32_t mgmt_event)
{
	switch (mgmt_event) {
		_CR(NET_EVENT_ETHERNET_CARRIER_ON);
		_CR(NET_EVENT_ETHERNET_CARRIER_OFF);
		_CR(NET_EVENT_IF_UP);
		_CR(NET_EVENT_IF_DOWN);
		_CR(NET_EVENT_IPV4_ADDR_ADD);
		_CR(NET_EVENT_IPV4_ADDR_DEL);
		_CR(NET_EVENT_IPV4_DHCP_START);
		_CR(NET_EVENT_IPV4_DHCP_BOUND);
		_CR(NET_EVENT_IPV4_DHCP_STOP);
		_CR(NET_EVENT_L4_CONNECTED);
		_CR(NET_EVENT_L4_DISCONNECTED);
	default:
		return "<unknown>";
	}
}

static const char *addr_type_to_str(enum net_addr_type addr_type)
{
	switch (addr_type) {
		_CR(NET_ADDR_ANY);
		_CR(NET_ADDR_AUTOCONF);
		_CR(NET_ADDR_DHCP);
		_CR(NET_ADDR_MANUAL);
		_CR(NET_ADDR_OVERRIDABLE);
	default:
		return "<unknown>";
	}
}

static void show_ipv4(struct net_if *iface)
{
	struct net_if_config *const ifcfg = &iface->config;

	for (uint_fast16_t i = 0; i < NET_IF_MAX_IPV4_ADDR; i++) {
		char buf[NET_IPV4_ADDR_LEN];

		LOG_INF("=== NET interface %p ===", iface);

		LOG_INF("Address: %s [addr type %s]",
				net_addr_ntop(AF_INET, &ifcfg->ip.ipv4->unicast[i].ipv4.address.in_addr, buf,
							  sizeof(buf)),
				addr_type_to_str(ifcfg->ip.ipv4->unicast[i].ipv4.addr_type));

		LOG_INF("Subnet:  %s",
				net_addr_ntop(AF_INET, &ifcfg->ip.ipv4->unicast->netmask, buf, sizeof(buf)));
		LOG_INF("Router:  %s",
				net_addr_ntop(AF_INET, &ifcfg->ip.ipv4->gw, buf, sizeof(buf)));

#if defined(CONFIG_NET_DHCPV4)
		if (ifcfg->ip.ipv4->unicast[i].ipv4.addr_type == NET_ADDR_DHCP) {
			LOG_INF("DHCPv4 Lease time: %u seconds [state: %s]", ifcfg->dhcpv4.lease_time,
					net_dhcpv4_state_name(ifcfg->dhcpv4.state));
		}
#endif
	}
}

static void net_event_handler(struct net_mgmt_event_callback *cb,
							  uint32_t mgmt_event,
							  struct net_if *iface)
{
	LOG_DBG("[face: %p] event: %s (%x)", iface, net_mgmt_event_to_str(mgmt_event),
			mgmt_event);

	switch (mgmt_event) {
	case NET_EVENT_ETHERNET_CARRIER_ON:
	case NET_EVENT_ETHERNET_CARRIER_OFF:
		break;

	case NET_EVENT_IF_UP:
		usb_iface_configure(iface);
		break;

	case NET_EVENT_IF_DOWN:
		usb_iface_deinit(iface);
		break;

	case NET_EVENT_IPV4_ADDR_ADD:
		show_ipv4(iface);
	case NET_EVENT_IPV4_ADDR_DEL:
	case NET_EVENT_IPV4_DHCP_START:
	case NET_EVENT_IPV4_DHCP_BOUND:
	case NET_EVENT_IPV4_DHCP_STOP:
	default:
		break;
	}
}

static void usb_iface_configure(struct net_if *iface)
{
	struct in_addr gw, my, nm;

	/* Configure the interface with a static IP address */
	net_addr_pton(AF_INET, CONFIG_COPRO_USB_ECM_IPV4_ADDR, &my);
	net_if_ipv4_addr_add(iface, &my, NET_ADDR_MANUAL, 0);

	net_addr_pton(AF_INET, CONFIG_COPRO_USB_ECM_IPV4_NETMASK, &nm);
	net_if_ipv4_set_netmask_by_addr(iface, &my, &nm);
	
    net_addr_pton(AF_INET, CONFIG_COPRO_USB_ECM_IPV4_GW, &gw);
	net_if_ipv4_set_gw(iface, &gw);
}

static void usb_iface_deinit(struct net_if *iface)
{
	net_if_ipv4_addr_rm(iface, &iface->config.ip.ipv4->unicast->ipv4.address.in_addr);
}