#include "text/grapheme.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "grapheme_vectors.h"
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"grapheme %d: %s\n",__LINE__,#x);exit(1); } } while (0)
int main(void) {
    size_t outputs[128],count;
    for (size_t i=0;i<sizeof(grapheme_vectors)/sizeof(grapheme_vectors[0]);++i) {
        CHECK(ae_text_graphemes(grapheme_vectors[i].text,grapheme_vectors[i].size,outputs,128,&count));
        if (count!=grapheme_vectors[i].count || memcmp(outputs,grapheme_vectors[i].offsets,count*sizeof(size_t))) {
            fprintf(stderr,"official vector %zu failed\n",i);return 1;
        }
        size_t measured;CHECK(ae_text_graphemes(grapheme_vectors[i].text,grapheme_vectors[i].size,NULL,0,&measured) && measured==count);
    }
    CHECK(ae_text_graphemes(NULL,0,outputs,128,&count) && count==1 && outputs[0]==0);
    static const unsigned char invalid[][5]={{0x80},{0xc0,0x80},{0xc1,0xbf},{0xe0,0x80,0x80},{0xed,0xa0,0x80},{0xf0,0x80,0x80,0x80},{0xf4,0x90,0x80,0x80},{0xf5,0x80,0x80,0x80},{0xe2,0x41,0x80},{0xc2},{0xe2,0x82},{0xf0,0x90,0x80}};
    static const size_t sizes[]={1,2,2,3,3,4,4,4,3,1,2,3};
    for (size_t i=0;i<sizeof(sizes)/sizeof(sizes[0]);++i) {
        size_t saved[128];memset(outputs,0x5a,sizeof(outputs));memcpy(saved,outputs,sizeof(outputs));
        CHECK(!ae_text_graphemes((const char *)invalid[i],sizes[i],outputs,128,&count) && !count);
        CHECK(!memcmp(outputs,saved,sizeof(outputs)));
        size_t at=0;uint32_t scalar=42;CHECK(!ae_utf8_next((const char *)invalid[i],sizes[i],&at,&scalar) && at==0 && scalar==42);
    }
    size_t saved[128];memset(outputs,0x33,sizeof(outputs));memcpy(saved,outputs,sizeof(outputs));
    CHECK(!ae_text_graphemes("abc",3,outputs,3,&count) && count==4 && !memcmp(outputs,saved,sizeof(outputs)));
    CHECK(!ae_text_graphemes(NULL,1,outputs,128,&count));
    CHECK(!ae_text_graphemes("a",1,NULL,1,&count));
    printf("graphemes: %zu complete Unicode 18 official vectors; strict UTF-8, empty spans, capacity and unchanged-error outputs passed\n",sizeof(grapheme_vectors)/sizeof(grapheme_vectors[0]));return 0;
}
