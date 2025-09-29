#!/usr/bin/env python3
"""
VR Telemetry System Test

This script tests the complete VR telemetry system by:
1. Starting the VR simulation
2. Starting the consumer
3. Verifying data flow
4. Running performance tests
"""

import subprocess
import time
import threading
import json
import requests
import psutil
import os
import sys
from datetime import datetime

class VRTelemetrySystemTest:
    def __init__(self):
        self.simulation_process = None
        self.consumer_process = None
        self.test_results = {
            'simulation_start': False,
            'consumer_start': False,
            'data_flow': False,
            'performance': {},
            'errors': []
        }
        
    def log(self, message):
        """Log test message with timestamp"""
        timestamp = datetime.now().strftime('%H:%M:%S')
        print(f"[{timestamp}] {message}")
        
    def check_dependencies(self):
        """Check if required dependencies are installed"""
        self.log("Checking dependencies...")
        
        # Check if RabbitMQ is running
        try:
            import pika
            connection = pika.BlockingConnection(
                pika.ConnectionParameters('localhost')
            )
            connection.close()
            self.log("✓ RabbitMQ is running")
        except Exception as e:
            self.log(f"✗ RabbitMQ not available: {e}")
            return False
            
        # Check if C compiler is available
        try:
            result = subprocess.run(['gcc', '--version'], capture_output=True, text=True)
            if result.returncode == 0:
                self.log("✓ GCC compiler available")
            else:
                self.log("✗ GCC compiler not found")
                return False
        except FileNotFoundError:
            self.log("✗ GCC compiler not found")
            return False
            
        # Check Python dependencies
        try:
            import numpy, matplotlib, pandas, plotly, dash
            self.log("✓ Python dependencies available")
        except ImportError as e:
            self.log(f"✗ Missing Python dependency: {e}")
            return False
            
        return True
        
    def build_simulation(self):
        """Build the VR simulation"""
        self.log("Building VR simulation...")
        
        try:
            # Clean and build
            subprocess.run(['make', 'clean'], check=True, cwd='.')
            result = subprocess.run(['make'], check=True, cwd='.', capture_output=True, text=True)
            
            if os.path.exists('bin/vr_telemetry_sim'):
                self.log("✓ VR simulation built successfully")
                return True
            else:
                self.log("✗ VR simulation build failed")
                return False
                
        except subprocess.CalledProcessError as e:
            self.log(f"✗ Build failed: {e.stderr}")
            return False
            
    def start_simulation(self, duration=30):
        """Start the VR simulation"""
        self.log(f"Starting VR simulation for {duration} seconds...")
        
        try:
            self.simulation_process = subprocess.Popen(
                ['./bin/vr_telemetry_sim', '-d', str(duration), '-f', '30'],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True
            )
            
            # Give it a moment to start
            time.sleep(2)
            
            if self.simulation_process.poll() is None:
                self.log("✓ VR simulation started")
                self.test_results['simulation_start'] = True
                return True
            else:
                stdout, stderr = self.simulation_process.communicate()
                self.log(f"✗ VR simulation failed to start: {stderr}")
                return False
                
        except Exception as e:
            self.log(f"✗ Failed to start simulation: {e}")
            return False
            
    def start_consumer(self):
        """Start the Python consumer"""
        self.log("Starting VR telemetry consumer...")
        
        try:
            self.consumer_process = subprocess.Popen(
                [sys.executable, 'python/vr_consumer.py'],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True
            )
            
            # Give it a moment to start
            time.sleep(2)
            
            if self.consumer_process.poll() is None:
                self.log("✓ VR consumer started")
                self.test_results['consumer_start'] = True
                return True
            else:
                stdout, stderr = self.consumer_process.communicate()
                self.log(f"✗ VR consumer failed to start: {stderr}")
                return False
                
        except Exception as e:
            self.log(f"✗ Failed to start consumer: {e}")
            return False
            
    def test_data_flow(self, duration=20):
        """Test data flow between simulation and consumer"""
        self.log(f"Testing data flow for {duration} seconds...")
        
        start_time = time.time()
        message_count = 0
        
        try:
            import pika
            connection = pika.BlockingConnection(pika.ConnectionParameters('localhost'))
            channel = connection.channel()
            
            # Declare queue to monitor messages
            result = channel.queue_declare(queue='', exclusive=True)
            queue_name = result.method.queue
            
            channel.queue_bind(exchange='vr_telemetry', queue=queue_name, routing_key='telemetry.data')
            
            def callback(ch, method, properties, body):
                nonlocal message_count
                message_count += 1
                if message_count % 50 == 0:
                    self.log(f"Received {message_count} messages...")
            
            channel.basic_consume(queue=queue_name, on_message_callback=callback, auto_ack=True)
            
            # Monitor for specified duration
            while time.time() - start_time < duration:
                connection.process_data_events(time_limit=1)
                
            connection.close()
            
            if message_count > 0:
                self.log(f"✓ Data flow test passed - received {message_count} messages")
                self.test_results['data_flow'] = True
                return True
            else:
                self.log("✗ No messages received during data flow test")
                return False
                
        except Exception as e:
            self.log(f"✗ Data flow test failed: {e}")
            return False
            
    def test_performance(self):
        """Test system performance"""
        self.log("Testing system performance...")
        
        # Monitor system resources
        cpu_usage = psutil.cpu_percent(interval=1)
        memory_usage = psutil.virtual_memory().percent
        
        self.test_results['performance'] = {
            'cpu_usage': cpu_usage,
            'memory_usage': memory_usage,
            'timestamp': datetime.now().isoformat()
        }
        
        self.log(f"System performance - CPU: {cpu_usage}%, Memory: {memory_usage}%")
        
        # Performance thresholds
        if cpu_usage > 80:
            self.log("⚠ High CPU usage detected")
        if memory_usage > 80:
            self.log("⚠ High memory usage detected")
            
        return True
        
    def cleanup(self):
        """Clean up test processes"""
        self.log("Cleaning up test processes...")
        
        if self.simulation_process and self.simulation_process.poll() is None:
            self.simulation_process.terminate()
            self.simulation_process.wait()
            self.log("✓ Simulation process terminated")
            
        if self.consumer_process and self.consumer_process.poll() is None:
            self.consumer_process.terminate()
            self.consumer_process.wait()
            self.log("✓ Consumer process terminated")
            
    def run_full_test(self):
        """Run complete system test"""
        self.log("Starting VR Telemetry System Test")
        self.log("=" * 50)
        
        try:
            # Check dependencies
            if not self.check_dependencies():
                self.log("✗ Dependency check failed")
                return False
                
            # Build simulation
            if not self.build_simulation():
                self.log("✗ Build failed")
                return False
                
            # Start simulation
            if not self.start_simulation(duration=30):
                self.log("✗ Simulation start failed")
                return False
                
            # Start consumer
            if not self.start_consumer():
                self.log("✗ Consumer start failed")
                return False
                
            # Test data flow
            if not self.test_data_flow(duration=20):
                self.log("✗ Data flow test failed")
                return False
                
            # Test performance
            self.test_performance()
            
            # Wait for simulation to complete
            if self.simulation_process:
                self.simulation_process.wait()
                
        except KeyboardInterrupt:
            self.log("Test interrupted by user")
            
        finally:
            self.cleanup()
            
        # Print test results
        self.log("=" * 50)
        self.log("Test Results:")
        self.log(f"  Simulation Start: {'✓' if self.test_results['simulation_start'] else '✗'}")
        self.log(f"  Consumer Start: {'✓' if self.test_results['consumer_start'] else '✗'}")
        self.log(f"  Data Flow: {'✓' if self.test_results['data_flow'] else '✗'}")
        
        if self.test_results['performance']:
            perf = self.test_results['performance']
            self.log(f"  CPU Usage: {perf['cpu_usage']}%")
            self.log(f"  Memory Usage: {perf['memory_usage']}%")
            
        # Overall result
        all_passed = (self.test_results['simulation_start'] and 
                     self.test_results['consumer_start'] and 
                     self.test_results['data_flow'])
        
        if all_passed:
            self.log("✓ All tests passed!")
            return True
        else:
            self.log("✗ Some tests failed")
            return False

def main():
    """Main test function"""
    test = VRTelemetrySystemTest()
    success = test.run_full_test()
    sys.exit(0 if success else 1)

if __name__ == '__main__':
    main()
