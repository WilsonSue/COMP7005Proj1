#!/usr/bin/env python3
"""
UDP Reliable Messaging Log Visualizer
Reads log files and generates visual statistics
"""

import re
import sys
import glob
from collections import defaultdict
from datetime import datetime

def find_log_files():
    """Find log files matching the patterns *Client.log, *Server.log, *Proxy.log"""
    client_logs = glob.glob('*Client.log')
    server_logs = glob.glob('*Server.log')
    proxy_logs = glob.glob('*Proxy.log')

    return {
        'client': client_logs,
        'server': server_logs,
        'proxy': proxy_logs
    }

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
        'delayed_s2c': 0,
        'delay_times_c2s': [],
        'delay_times_s2c': []
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
            delay_match = re.search(r'DELAYED (\d+)ms', msg)
            if delay_match:
                stats['delay_times_c2s'].append(int(delay_match.group(1)))
        elif 'S->C: DELAYED' in msg:
            stats['delayed_s2c'] += 1
            delay_match = re.search(r'DELAYED (\d+)ms', msg)
            if delay_match:
                stats['delay_times_s2c'].append(int(delay_match.group(1)))

    return stats

def print_bar_chart(label, value, max_value, width=50):
    """Print a simple ASCII bar chart"""
    if max_value == 0:
        bar_width = 0
    else:
        bar_width = int((value / max_value) * width)

    bar = '█' * bar_width
    print(f"{label:25s} | {bar} {value}")

def extract_test_name(filename):
    """Extract test name from filename (e.g., 'NoProxy' from 'NoProxyClient.log')"""
    # Remove 'Client.log', 'Server.log', or 'Proxy.log' suffix
    name = filename.replace('Client.log', '').replace('Server.log', '').replace('Proxy.log', '')
    return name if name else 'default'

def analyze_test_set(client_file, server_file, proxy_file=None):
    """Analyze a complete test set (client, server, and optionally proxy)"""
    test_name = extract_test_name(client_file) if client_file else \
        extract_test_name(server_file) if server_file else "Unknown"

    print("=" * 70)
    print(f"TEST: {test_name}")
    print("=" * 70)
    print()

    # Parse logs
    client_events = parse_log_file(client_file) if client_file else []
    server_events = parse_log_file(server_file) if server_file else []
    proxy_events = parse_log_file(proxy_file) if proxy_file else []

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

        if len(client_stats['sequences']) > 0:
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

        if proxy_stats['delay_times_c2s']:
            avg_delay = sum(proxy_stats['delay_times_c2s']) / len(proxy_stats['delay_times_c2s'])
            min_delay = min(proxy_stats['delay_times_c2s'])
            max_delay = max(proxy_stats['delay_times_c2s'])
            print(f"  Delay range:            {min_delay}-{max_delay}ms (avg: {avg_delay:.1f}ms)")

        print()
        print(f"Server->Client packets:   {proxy_stats['server_to_client']}")
        print(f"  Dropped:                {proxy_stats['dropped_s2c']}")
        print(f"  Delayed:                {proxy_stats['delayed_s2c']}")

        if proxy_stats['server_to_client'] > 0:
            drop_rate = (proxy_stats['dropped_s2c'] / proxy_stats['server_to_client']) * 100
            print(f"  Drop rate:              {drop_rate:.1f}%")

        if proxy_stats['delay_times_s2c']:
            avg_delay = sum(proxy_stats['delay_times_s2c']) / len(proxy_stats['delay_times_s2c'])
            min_delay = min(proxy_stats['delay_times_s2c'])
            max_delay = max(proxy_stats['delay_times_s2c'])
            print(f"  Delay range:            {min_delay}-{max_delay}ms (avg: {avg_delay:.1f}ms)")

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
    print()

def main():
    # Find all matching log files
    log_files = find_log_files()

    if not any(log_files.values()):
        print("No log files found matching patterns: *Client.log, *Server.log, *Proxy.log")
        print("\nExpected log file formats:")
        print("  - NoProxyClient.log, NoProxyServer.log")
        print("  - PerfectNetworkClient.log, PerfectNetworkServer.log, PerfectProxy.log")
        print("  - 5DropClient.log, 5DropServer.log, 5DropProxy.log")
        print("  - 10DropClient.log, 10DropServer.log, 10DropProxy.log")
        print("  - 50DelayClient.log, 50DelayServer.log, 50DelayProxy.log")
        print("  - 100DelayClient.log, 100DelayServer.log, 100DelayProxy.log")
        print("  - 50Drop50DelayClient.log, 50Drop50DelayServer.log, 50Drop50DelayProxy.log")
        print("\nCurrent directory:", glob.os.getcwd())
        return

    print("=" * 70)
    print("UDP RELIABLE MESSAGING - LOG ANALYSIS")
    print("=" * 70)
    print()

    # Display found files
    print("Found log files:")
    if log_files['client']:
        print(f"  Client logs: {', '.join(sorted(log_files['client']))}")
    if log_files['server']:
        print(f"  Server logs: {', '.join(sorted(log_files['server']))}")
    if log_files['proxy']:
        print(f"  Proxy logs:  {', '.join(sorted(log_files['proxy']))}")
    print()

    # Group logs by test name
    test_sets = defaultdict(lambda: {'client': None, 'server': None, 'proxy': None})

    for client_log in log_files['client']:
        test_name = extract_test_name(client_log)
        test_sets[test_name]['client'] = client_log

    for server_log in log_files['server']:
        test_name = extract_test_name(server_log)
        test_sets[test_name]['server'] = server_log

    for proxy_log in log_files['proxy']:
        test_name = extract_test_name(proxy_log)
        test_sets[test_name]['proxy'] = proxy_log

    # Define preferred order for tests based on README
    preferred_order = [
        'NoProxy',
        'PerfectNetwork',
        '5Drop',
        '10Drop',
        '50Delay',
        '100Delay',
        '50Drop50Delay'
    ]

    # Sort test names: preferred order first, then alphabetically
    def sort_key(test_name):
        try:
            return (0, preferred_order.index(test_name))
        except ValueError:
            return (1, test_name)

    sorted_test_names = sorted(test_sets.keys(), key=sort_key)

    # Analyze each test set
    for test_name in sorted_test_names:
        test_set = test_sets[test_name]
        analyze_test_set(
            test_set['client'],
            test_set['server'],
            test_set['proxy']
        )

if __name__ == '__main__':
    main()