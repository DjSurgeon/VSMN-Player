extern "C" {
#include <libavcodec/avcodec.h>
}
int main() { AVCodecContext ctx; return (int)ctx.ch_layout.nb_channels; }
