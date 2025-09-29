#include "vr_telemetry.h"
#include <amqp.h>
#include <amqp_tcp_socket.h>
#include <amqp_framing.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// RabbitMQ connection state
static amqp_connection_state_t g_conn = NULL;
static amqp_socket_t *g_socket = NULL;
static bool g_connected = false;

// Connection parameters
static char g_host[256] = "localhost";
static int g_port = 5672;
static char g_username[64] = "guest";
static char g_password[64] = "guest";
static char g_vhost[64] = "/";
static char g_exchange[64] = "vr_telemetry";
static char g_routing_key[64] = "telemetry.data";

// Initialize RabbitMQ connection
int vr_rabbitmq_init(const char *host, int port, const char *username, 
                     const char *password, const char *vhost, 
                     const char *exchange, const char *routing_key) {
    if (host) strncpy(g_host, host, sizeof(g_host) - 1);
    if (username) strncpy(g_username, username, sizeof(g_username) - 1);
    if (password) strncpy(g_password, password, sizeof(g_password) - 1);
    if (vhost) strncpy(g_vhost, vhost, sizeof(g_vhost) - 1);
    if (exchange) strncpy(g_exchange, exchange, sizeof(g_exchange) - 1);
    if (routing_key) strncpy(g_routing_key, routing_key, sizeof(g_routing_key) - 1);
    if (port > 0) g_port = port;
    
    // Create connection
    g_conn = amqp_new_connection();
    if (!g_conn) {
        fprintf(stderr, "Failed to create RabbitMQ connection\n");
        return -1;
    }
    
    // Create socket
    g_socket = amqp_tcp_socket_new(g_conn);
    if (!g_socket) {
        fprintf(stderr, "Failed to create RabbitMQ socket\n");
        amqp_destroy_connection(g_conn);
        return -1;
    }
    
    // Connect to broker
    int status = amqp_socket_open(g_socket, g_host, g_port);
    if (status) {
        fprintf(stderr, "Failed to open RabbitMQ socket: %s\n", amqp_error_string2(status));
        amqp_destroy_connection(g_conn);
        return -1;
    }
    
    // Login
    amqp_rpc_reply_t reply = amqp_login(g_conn, g_vhost, 0, 131072, 0, AMQP_SASL_METHOD_PLAIN, 
                                        g_username, g_password);
    if (reply.reply_type != AMQP_RESPONSE_NORMAL) {
        fprintf(stderr, "Failed to login to RabbitMQ\n");
        amqp_destroy_connection(g_conn);
        return -1;
    }
    
    // Open channel
    amqp_channel_open(g_conn, 1);
    reply = amqp_get_rpc_reply(g_conn);
    if (reply.reply_type != AMQP_RESPONSE_NORMAL) {
        fprintf(stderr, "Failed to open RabbitMQ channel\n");
        amqp_destroy_connection(g_conn);
        return -1;
    }
    
    // Declare exchange
    amqp_exchange_declare(g_conn, 1, amqp_cstring_bytes(g_exchange), 
                         amqp_cstring_bytes("topic"), 0, 1, 0, 0, amqp_empty_table);
    reply = amqp_get_rpc_reply(g_conn);
    if (reply.reply_type != AMQP_RESPONSE_NORMAL) {
        fprintf(stderr, "Failed to declare RabbitMQ exchange\n");
        amqp_destroy_connection(g_conn);
        return -1;
    }
    
    g_connected = true;
    printf("Connected to RabbitMQ at %s:%d\n", g_host, g_port);
    return 0;
}

// Send telemetry packet to RabbitMQ
int vr_rabbitmq_send_telemetry(const vr_telemetry_packet_t *packet) {
    if (!g_connected || !packet) {
        return -1;
    }
    
    // Serialize packet to JSON-like format
    char message[2048];
    int len = snprintf(message, sizeof(message),
        "{"
        "\"timestamp_us\":%lu,"
        "\"frame_id\":%u,"
        "\"head_position\":{\"x\":%.6f,\"y\":%.6f,\"z\":%.6f},"
        "\"head_orientation\":{\"x\":%.6f,\"y\":%.6f,\"z\":%.6f,\"w\":%.6f},"
        "\"head_acceleration\":{\"x\":%.6f,\"y\":%.6f,\"z\":%.6f},"
        "\"head_angular_velocity\":{\"x\":%.6f,\"y\":%.6f,\"z\":%.6f},"
        "\"left_eye\":{\"x\":%.6f,\"y\":%.6f,\"pupil_diameter\":%.6f,\"is_blinking\":%s},"
        "\"right_eye\":{\"x\":%.6f,\"y\":%.6f,\"pupil_diameter\":%.6f,\"is_blinking\":%s},"
        "\"left_hand\":{\"x\":%.6f,\"y\":%.6f,\"z\":%.6f,\"orientation\":{\"x\":%.6f,\"y\":%.6f,\"z\":%.6f,\"w\":%.6f},\"grip_strength\":%.6f,\"is_tracking\":%s},"
        "\"right_hand\":{\"x\":%.6f,\"y\":%.6f,\"z\":%.6f,\"orientation\":{\"x\":%.6f,\"y\":%.6f,\"z\":%.6f,\"w\":%.6f},\"grip_strength\":%.6f,\"is_tracking\":%s},"
        "\"cpu_usage\":%.2f,"
        "\"gpu_usage\":%.2f,"
        "\"temperature\":%.2f,"
        "\"battery_level\":%u,"
        "\"is_connected\":%s"
        "}",
        packet->timestamp_us,
        packet->frame_id,
        packet->head_position.x, packet->head_position.y, packet->head_position.z,
        packet->head_orientation.x, packet->head_orientation.y, packet->head_orientation.z, packet->head_orientation.w,
        packet->head_acceleration.x, packet->head_acceleration.y, packet->head_acceleration.z,
        packet->head_angular_velocity.x, packet->head_angular_velocity.y, packet->head_angular_velocity.z,
        packet->left_eye.x, packet->left_eye.y, packet->left_eye.pupil_diameter, packet->left_eye.is_blinking ? "true" : "false",
        packet->right_eye.x, packet->right_eye.y, packet->right_eye.pupil_diameter, packet->right_eye.is_blinking ? "true" : "false",
        packet->left_hand.x, packet->left_hand.y, packet->left_hand.z,
        packet->left_hand.orientation.x, packet->left_hand.orientation.y, packet->left_hand.orientation.z, packet->left_hand.orientation.w,
        packet->left_hand.grip_strength, packet->left_hand.is_tracking ? "true" : "false",
        packet->right_hand.x, packet->right_hand.y, packet->right_hand.z,
        packet->right_hand.orientation.x, packet->right_hand.orientation.y, packet->right_hand.orientation.z, packet->right_hand.orientation.w,
        packet->right_hand.grip_strength, packet->right_hand.is_tracking ? "true" : "false",
        packet->cpu_usage,
        packet->gpu_usage,
        packet->temperature,
        packet->battery_level,
        packet->is_connected ? "true" : "false"
    );
    
    if (len >= sizeof(message)) {
        fprintf(stderr, "Message too large for buffer\n");
        return -1;
    }
    
    // Publish message
    amqp_basic_properties_t props;
    props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG;
    props.content_type = amqp_cstring_bytes("application/json");
    props.delivery_mode = 2; // Persistent message
    
    int status = amqp_basic_publish(g_conn, 1, amqp_cstring_bytes(g_exchange),
                                   amqp_cstring_bytes(g_routing_key), 0, 0,
                                   &props, amqp_cstring_bytes(message));
    
    if (status != AMQP_STATUS_OK) {
        fprintf(stderr, "Failed to publish message: %s\n", amqp_error_string2(status));
        return -1;
    }
    
    return 0;
}

// Check if connected
bool vr_rabbitmq_is_connected(void) {
    return g_connected;
}

// Close RabbitMQ connection
void vr_rabbitmq_close(void) {
    if (g_connected) {
        amqp_channel_close(g_conn, 1, AMQP_REPLY_SUCCESS);
        amqp_connection_close(g_conn, AMQP_REPLY_SUCCESS);
        amqp_destroy_connection(g_conn);
        g_connected = false;
        printf("Disconnected from RabbitMQ\n");
    }
}

// Reconnect to RabbitMQ
int vr_rabbitmq_reconnect(void) {
    vr_rabbitmq_close();
    return vr_rabbitmq_init(g_host, g_port, g_username, g_password, g_vhost, g_exchange, g_routing_key);
}
