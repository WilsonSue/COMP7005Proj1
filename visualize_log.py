#!/usr/bin/env python3
"""
UDP Reliable Messaging Log Visualizer
Reads log files and generates visual statistics
"""

import re
import sys
from collections import defaultdict
from datetime import datetime

def parse_log_file(filename):
    """Parse a log file and extract events"""
    events = []

    try:
        with open(filename, 'r') as f:
            for line in f:
                match = re.match(r'\[([\d\-: ]+)\] (.+)', line.strip())
                if match:
                    timestamp_str, message = match.groups()
                    try:
                        timestamp = datetime.strptime(timestamp_str, '%Y-%m-%d %H:%M:%S')
                        events.append({
                            'timestamp': timestamp,
                            'message': message
                        })
                    except ValueError:
                        pass
    except FileNotFoundError:
        print(f"Warning: {filename} not found")
        return []

    return events

def analyze_client_log(events):
    """Analyze client log for statistics"""
    stats = {
        'total_sent': 0,
        'total_acked': 0,
        'total_failed': 0,
        'total_timeouts': 0,
        'retransmissions': defaultdict(int),
        'sequences': set()
    }

    for event in events:
        msg = event['message']

        if 'SEND:' in msg:
            stats['total_sent'] += 1
            seq_match = re.search(r'seq=(\d+)', msg)
            if seq_match:
                stats['sequences'].add(int(seq_match.group(1)))

        if 'ACK_RECV:' in msg:
            stats['total_acked'] += 1

        if 'FAILED:' in msg:
            stats['total_failed'] += 1

        if 'TIMEOUT:' in msg:
            stats['total_timeouts'] += 1
            attempt_match = re.search(r'attempt=(\d+)', msg)
            if attempt_match:
                stats['retransmissions'][int(attempt_match.group(1))] += 1

    return stats

def analyze_server_log(events):
    """Analyze server log for statistics"""
    stats = {
        'total_received': 0,
        'total_acks_sent': 0,
        'unique_sequences': set()
    }

    for event in events:
        msg = event['message']

        if 'RECV:' in msg and 'payload=' in msg:
            stats['total_received'] += 1
            seq_match = re.search(r'seq=(\d+)', msg)
            if seq_match:
                stats['unique_sequences'].add(int(seq_match.group(1)))

        if 'ACK_SEND:' in msg:
            stats['total_acks_sent'] += 1

    return stats

def analyze_proxy_log(events):
    """Analyze proxy log for statistics"""
    stats = {
        'client_to_server': 0,
        'server_to_client': 0,
        'dropped_c2s': 0,
        'dropped_s2c': 0,
        'delayed_c2s': 0,
        'delayed_s2c': 0
    }

    for event in events:
        msg = event['message']

        if 'C->S: Received' in msg:
            stats['client_to_server'] += 1
        elif 'S->C: Received' in msg:
            stats['server_to_client'] += 1

        if 'C->S: DROPPED' in msg:
            stats['dropped_c2s'] += 1
        elif 'S->C: DROPPED' in msg:
            stats['dropped_s2c'] += 1

        if 'C->S: DELAYED' in msg:
            stats['delayed_c2s'] += 1
        elif 'S->C: DELAYED' in msg:
            stats['delayed_s2c'] += 1

    return stats

def print_bar_chart(label, value, max_value, width=50):
    """Print a simple ASCII bar chart"""
    if max_value == 0:
        bar_width = 0
    else:
        bar_width = int((value / max_value) * width)

    bar = '█' * bar_width
    print(f"{label:25s} | {bar} {value}")

def main():
    print("=" * 70)
    print("UDP RELIABLE MESSAGING - LOG ANALYSIS")
    print("=" * 70)
    print()

    # Parse logs
    client_events = parse_log_file('client.log')
    server_events = parse_log_file('server.log')
    proxy_events = parse_log_file('proxy.log')

    # Analyze client
    if client_events:
        print("CLIENT STATISTICS:")
        print("-" * 70)
        client_stats = analyze_client_log(client_events)

        print(f"Unique messages sent:     {len(client_stats['sequences'])}")
        print(f"Total transmissions:      {client_stats['total_sent']}")
        print(f"Successful ACKs:          {client_stats['total_acked']}")
        print(f"Failed messages:          {client_stats['total_failed']}")
        print(f"Timeouts:                 {client_stats['total_timeouts']}")

        if client_stats['total_sent'] > 0:
            success_rate = (client_stats['total_acked'] / len(client_stats['sequences'])) * 100
            print(f"Success rate:             {success_rate:.1f}%")

        if client_stats['retransmissions']:
            print("\nRetransmission attempts:")
            max_attempts = max(client_stats['retransmissions'].values()) if client_stats['retransmissions'] else 1
            for attempt in sorted(client_stats['retransmissions'].keys()):
                count = client_stats['retransmissions'][attempt]
                print_bar_chart(f"  Attempt {attempt}", count, max_attempts)

        print()

    # Analyze server
    if server_events:
        print("SERVER STATISTICS:")
        print("-" * 70)
        server_stats = analyze_server_log(server_events)

        print(f"Messages received:        {server_stats['total_received']}")
        print(f"Unique sequences:         {len(server_stats['unique_sequences'])}")
        print(f"ACKs sent:                {server_stats['total_acks_sent']}")
        print()

    # Analyze proxy
    if proxy_events:
        print("PROXY STATISTICS:")
        print("-" * 70)
        proxy_stats = analyze_proxy_log(proxy_events)

        print(f"Client->Server packets:   {proxy_stats['client_to_server']}")
        print(f"  Dropped:                {proxy_stats['dropped_c2s']}")
        print(f"  Delayed:                {proxy_stats['delayed_c2s']}")

        if proxy_stats['client_to_server'] > 0:
            drop_rate = (proxy_stats['dropped_c2s'] / proxy_stats['client_to_server']) * 100
            print(f"  Drop rate:              {drop_rate:.1f}%")

        print()
        print(f"Server->Client packets:   {proxy_stats['server_to_client']}")
        print(f"  Dropped:                {proxy_stats['dropped_s2c']}")
        print(f"  Delayed:                {proxy_stats['delayed_s2c']}")

        if proxy_stats['server_to_client'] > 0:
            drop_rate = (proxy_stats['dropped_s2c'] / proxy_stats['server_to_client']) * 100
            print(f"  Drop rate:              {drop_rate:.1f}%")

        print()

    # Summary
    print("=" * 70)
    if client_events and server_events:
        client_stats = analyze_client_log(client_events)
        server_stats = analyze_server_log(server_events)

        messages_sent = len(client_stats['sequences'])
        messages_received = len(server_stats['unique_sequences'])

        print(f"OVERALL: {messages_received}/{messages_sent} messages delivered successfully")

        if messages_sent > 0:
            delivery_rate = (messages_received / messages_sent) * 100
            print(f"Delivery rate: {delivery_rate:.1f}%")

    print("=" * 70)

if __name__ == '__main__':
    main()