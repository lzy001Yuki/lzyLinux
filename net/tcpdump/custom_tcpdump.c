#include <pcap.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct capture_ctx {
    void *buffer;
    size_t buffer_size;
    size_t bytes_written;
};

static void packet_handler(u_char *user_data, const struct pcap_pkthdr *pkthdr, const u_char *packet) {
    struct capture_ctx *ctx = (struct capture_ctx *)user_data;
    
    if (ctx->bytes_written + pkthdr->caplen > ctx->buffer_size) {
        return; 
    }
    
    memcpy(ctx->buffer + ctx->bytes_written, packet, pkthdr->caplen);
    ctx->bytes_written += pkthdr->caplen;
}


int custom_tcpdump_capture(const char* iface, const char* custom_filter, void* buffer, size_t buffer_size) {
    char errbf[PCAP_ERRBUF_SIZE];
    struct capture_ctx ctx = {
        .buffer = buffer,
        .buffer_size = buffer_size,
        .bytes_written = 0
    };
    pcap_t * handle = pcap_open_live(iface, BUFSIZ, 1, 1000, errbf);
    if (handle == NULL) {
        printf("ERROR: failed to fetch device\n");
    }
    struct bpf_program fp;
    bpf_u_int32 net, mask;
    if (pcap_compile(handle, &fp, custom_filter, 0, net) == -1) {
        fprintf( "ERROR: failed to compile expression\n");
        pcap_close(handle);
        return -1;
    }

    if (pcap_setfilter(handle, &fp) == -1) {
        printf("ERROR: failed to set filter");
        pcap_freecode(&fp);
        pcap_close(handle);
        return -1;
    }

    int ret = pcap_loop(handle, -1, packet_handler, (u_char *)&ctx);
    
    pcap_freecode(&fp);
    pcap_close(handle);
    
    return ret;
}

int main(int argc, char* argv[]) {
    char* iface = NULL;
    char* filter = NULL;
    char buffer[65536] = {0}; 
    
    if (argc < 2) {
        printf("wrong input form\n");
        return 1;
    }
    
    iface = argv[1];
    filter = argv[2];

    int result = custom_tcpdump_capture(iface, filter, buffer, sizeof(buffer));
    
    if (result == 0) {
        printf("Success\n");
    } else {
        printf("Fail\n");
    }
    
    return 0;
}