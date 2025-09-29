#!/usr/bin/env python3
"""
VR Telemetry Consumer

This script consumes VR telemetry data from RabbitMQ and provides
real-time visualization and analysis capabilities.
"""

import json
import time
import threading
import signal
import sys
from collections import deque
from datetime import datetime, timedelta
import argparse

import pika
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
import tkinter as tk
from tkinter import ttk
import pandas as pd
import plotly.graph_objects as go
from plotly.subplots import make_subplots
import dash
from dash import dcc, html, Input, Output, dash_table
import dash_bootstrap_components as dbc
import psutil

class VRTelemetryConsumer:
    def __init__(self, host='localhost', port=5672, username='guest', password='guest',
                 vhost='/', exchange='vr_telemetry', routing_key='telemetry.data'):
        self.host = host
        self.port = port
        self.username = username
        self.password = password
        self.vhost = vhost
        self.exchange = exchange
        self.routing_key = routing_key
        
        self.connection = None
        self.channel = None
        self.running = False
        
        # Data storage
        self.telemetry_data = deque(maxlen=1000)  # Keep last 1000 data points
        self.stats = {
            'total_messages': 0,
            'start_time': None,
            'last_message_time': None,
            'message_rate': 0.0
        }
        
        # Performance metrics
        self.cpu_usage_history = deque(maxlen=100)
        self.gpu_usage_history = deque(maxlen=100)
        self.temperature_history = deque(maxlen=100)
        self.battery_history = deque(maxlen=100)
        
    def connect(self):
        """Connect to RabbitMQ"""
        try:
            credentials = pika.PlainCredentials(self.username, self.password)
            parameters = pika.ConnectionParameters(
                host=self.host,
                port=self.port,
                virtual_host=self.vhost,
                credentials=credentials
            )
            
            self.connection = pika.BlockingConnection(parameters)
            self.channel = self.connection.channel()
            
            # Declare exchange
            self.channel.exchange_declare(exchange=self.exchange, exchange_type='topic', durable=True)
            
            # Declare queue
            result = self.channel.queue_declare(queue='', exclusive=True)
            queue_name = result.method.queue
            
            # Bind queue to exchange
            self.channel.queue_bind(exchange=self.exchange, queue=queue_name, routing_key=self.routing_key)
            
            print(f"Connected to RabbitMQ at {self.host}:{self.port}")
            print(f"Listening to exchange: {self.exchange}, routing key: {self.routing_key}")
            
            return True
            
        except Exception as e:
            print(f"Failed to connect to RabbitMQ: {e}")
            return False
    
    def process_message(self, ch, method, properties, body):
        """Process incoming telemetry message"""
        try:
            data = json.loads(body.decode('utf-8'))
            
            # Add timestamp
            data['received_at'] = datetime.now().isoformat()
            
            # Store data
            self.telemetry_data.append(data)
            
            # Update statistics
            self.stats['total_messages'] += 1
            if self.stats['start_time'] is None:
                self.stats['start_time'] = datetime.now()
            self.stats['last_message_time'] = datetime.now()
            
            # Calculate message rate
            if self.stats['start_time']:
                elapsed = (datetime.now() - self.stats['start_time']).total_seconds()
                if elapsed > 0:
                    self.stats['message_rate'] = self.stats['total_messages'] / elapsed
            
            # Update performance metrics
            self.cpu_usage_history.append(data.get('cpu_usage', 0))
            self.gpu_usage_history.append(data.get('gpu_usage', 0))
            self.temperature_history.append(data.get('temperature', 0))
            self.battery_history.append(data.get('battery_level', 0))
            
            # Print status every 100 messages
            if self.stats['total_messages'] % 100 == 0:
                print(f"Processed {self.stats['total_messages']} messages "
                      f"(Rate: {self.stats['message_rate']:.1f} msg/s)")
            
        except Exception as e:
            print(f"Error processing message: {e}")
    
    def start_consuming(self):
        """Start consuming messages"""
        if not self.connect():
            return False
        
        self.running = True
        
        # Set up consumer
        self.channel.basic_consume(
            queue='',
            on_message_callback=self.process_message,
            auto_ack=True
        )
        
        print("Starting to consume messages... (Press Ctrl+C to stop)")
        
        try:
            self.channel.start_consuming()
        except KeyboardInterrupt:
            print("\nStopping consumer...")
            self.stop_consuming()
        
        return True
    
    def stop_consuming(self):
        """Stop consuming messages"""
        self.running = False
        if self.channel:
            self.channel.stop_consuming()
        if self.connection:
            self.connection.close()
        print("Consumer stopped.")
    
    def get_latest_data(self, count=1):
        """Get latest telemetry data"""
        if not self.telemetry_data:
            return None
        return list(self.telemetry_data)[-count:] if count > 1 else self.telemetry_data[-1]
    
    def get_statistics(self):
        """Get consumer statistics"""
        return self.stats.copy()
    
    def export_data(self, filename=None):
        """Export telemetry data to CSV"""
        if not self.telemetry_data:
            print("No data to export")
            return False
        
        if filename is None:
            filename = f"vr_telemetry_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"
        
        df = pd.DataFrame(list(self.telemetry_data))
        df.to_csv(filename, index=False)
        print(f"Data exported to {filename}")
        return True

class VRTelemetryVisualizer:
    def __init__(self, consumer):
        self.consumer = consumer
        self.root = tk.Tk()
        self.root.title("VR Telemetry Monitor")
        self.root.geometry("1200x800")
        
        # Create matplotlib figure
        self.fig, self.axes = plt.subplots(2, 2, figsize=(12, 8))
        self.fig.suptitle('VR Telemetry Real-time Monitor')
        
        # Initialize plots
        self.setup_plots()
        
        # Create GUI elements
        self.setup_gui()
        
        # Start animation
        self.ani = animation.FuncAnimation(self.fig, self.update_plots, interval=1000, blit=False)
        
    def setup_plots(self):
        """Setup matplotlib plots"""
        # CPU/GPU Usage
        self.axes[0, 0].set_title('CPU/GPU Usage (%)')
        self.axes[0, 0].set_ylabel('Usage %')
        self.axes[0, 0].grid(True)
        
        # Temperature
        self.axes[0, 1].set_title('Temperature (°C)')
        self.axes[0, 1].set_ylabel('Temperature °C')
        self.axes[0, 1].grid(True)
        
        # Battery Level
        self.axes[1, 0].set_title('Battery Level (%)')
        self.axes[1, 0].set_ylabel('Battery %')
        self.axes[1, 0].grid(True)
        
        # Head Position
        self.axes[1, 1].set_title('Head Position (m)')
        self.axes[1, 1].set_ylabel('Position (m)')
        self.axes[1, 1].grid(True)
        
    def setup_gui(self):
        """Setup GUI elements"""
        # Create canvas
        canvas = FigureCanvasTkAgg(self.fig, self.root)
        canvas.draw()
        canvas.get_tk_widget().pack(side=tk.TOP, fill=tk.BOTH, expand=True)
        
        # Create control panel
        control_frame = ttk.Frame(self.root)
        control_frame.pack(side=tk.BOTTOM, fill=tk.X, padx=5, pady=5)
        
        # Statistics labels
        self.stats_label = ttk.Label(control_frame, text="Waiting for data...")
        self.stats_label.pack(side=tk.LEFT)
        
        # Export button
        export_btn = ttk.Button(control_frame, text="Export Data", command=self.export_data)
        export_btn.pack(side=tk.RIGHT)
        
    def update_plots(self, frame):
        """Update plots with latest data"""
        if not self.consumer.telemetry_data:
            return
        
        # Get latest data
        latest_data = self.consumer.get_latest_data()
        if not latest_data:
            return
        
        # Clear axes
        for ax in self.axes.flat:
            ax.clear()
        
        # Update plots
        self.setup_plots()
        
        # CPU/GPU Usage
        if len(self.consumer.cpu_usage_history) > 1:
            self.axes[0, 0].plot(list(self.consumer.cpu_usage_history), label='CPU', color='blue')
            self.axes[0, 0].plot(list(self.consumer.gpu_usage_history), label='GPU', color='red')
            self.axes[0, 0].legend()
        
        # Temperature
        if len(self.consumer.temperature_history) > 1:
            self.axes[0, 1].plot(list(self.consumer.temperature_history), color='orange')
        
        # Battery Level
        if len(self.consumer.battery_history) > 1:
            self.axes[1, 0].plot(list(self.consumer.battery_history), color='green')
        
        # Head Position
        if 'head_position' in latest_data:
            pos = latest_data['head_position']
            self.axes[1, 1].plot([pos['x']], [pos['y']], 'bo', markersize=8)
            self.axes[1, 1].set_xlim(pos['x'] - 0.5, pos['x'] + 0.5)
            self.axes[1, 1].set_ylim(pos['y'] - 0.5, pos['y'] + 0.5)
        
        # Update statistics
        stats = self.consumer.get_statistics()
        stats_text = f"Messages: {stats['total_messages']} | Rate: {stats['message_rate']:.1f} msg/s"
        self.stats_label.config(text=stats_text)
        
    def export_data(self):
        """Export data to CSV"""
        self.consumer.export_data()
        
    def run(self):
        """Run the visualizer"""
        self.root.mainloop()

def signal_handler(sig, frame):
    """Handle Ctrl+C gracefully"""
    print("\nShutting down...")
    sys.exit(0)

def main():
    parser = argparse.ArgumentParser(description='VR Telemetry Consumer')
    parser.add_argument('--host', default='localhost', help='RabbitMQ host')
    parser.add_argument('--port', type=int, default=5672, help='RabbitMQ port')
    parser.add_argument('--username', default='guest', help='RabbitMQ username')
    parser.add_argument('--password', default='guest', help='RabbitMQ password')
    parser.add_argument('--vhost', default='/', help='RabbitMQ vhost')
    parser.add_argument('--exchange', default='vr_telemetry', help='RabbitMQ exchange')
    parser.add_argument('--routing-key', default='telemetry.data', help='RabbitMQ routing key')
    parser.add_argument('--visualize', action='store_true', help='Enable real-time visualization')
    parser.add_argument('--export-interval', type=int, default=0, help='Auto-export interval in seconds (0 = disabled)')
    
    args = parser.parse_args()
    
    # Set up signal handler
    signal.signal(signal.SIGINT, signal_handler)
    
    # Create consumer
    consumer = VRTelemetryConsumer(
        host=args.host,
        port=args.port,
        username=args.username,
        password=args.password,
        vhost=args.vhost,
        exchange=args.exchange,
        routing_key=args.routing_key
    )
    
    if args.visualize:
        # Run with visualization
        visualizer = VRTelemetryVisualizer(consumer)
        
        # Start consumer in separate thread
        consumer_thread = threading.Thread(target=consumer.start_consuming)
        consumer_thread.daemon = True
        consumer_thread.start()
        
        # Run visualizer
        visualizer.run()
    else:
        # Run without visualization
        consumer.start_consuming()
    
    # Auto-export if enabled
    if args.export_interval > 0:
        def auto_export():
            while consumer.running:
                time.sleep(args.export_interval)
                if consumer.telemetry_data:
                    consumer.export_data()
        
        export_thread = threading.Thread(target=auto_export)
        export_thread.daemon = True
        export_thread.start()

if __name__ == '__main__':
    main()
