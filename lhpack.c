#define LUA_LIB

#include <core.h>
#include <nghttp2/nghttp2.h>


struct hpack_ctx_t {
  size_t tsize; size_t closed;
  nghttp2_hd_deflater *encoder;
  nghttp2_hd_inflater *decoder;
};


static int hpack_encode(lua_State *L) {
  struct hpack_ctx_t *hctx = luaL_checkudata(L, 1, "__HPACK__");
  if (!hctx)
    return luaL_error(L, "HPACK encode: Invalid string buffer.");

  nghttp2_nv nList[256];

  int index = 0;
  lua_pushnil(L);
  while (lua_next(L, 2)) {
    size_t namelen, valuelen;
    const char* name = lua_tolstring(L, -2, &namelen);
    const char* value = lua_tolstring(L, -1, &valuelen);
    nList[index] = (nghttp2_nv){
      .name = (uint8_t*)name, .value = (uint8_t*)value,
      .namelen = namelen,     .valuelen = valuelen,
      .flags = NGHTTP2_NV_FLAG_NO_COPY_NAME | NGHTTP2_NV_FLAG_NO_COPY_VALUE,
      // .flags = NGHTTP2_NV_FLAG_NONE,
    };
    lua_pop(L, 1);
    index++;
  }

  size_t bsize = nghttp2_hd_deflate_bound(hctx->encoder, nList, index);
  char buf[bsize];
  memset(buf, 0x0, bsize);

  size_t ret = nghttp2_hd_deflate_hd(hctx->encoder, (uint8_t *)buf, bsize, nList, index);
  if (ret < 0)
    return luaL_error(L, "HPACK encode failed with error: %d, %s", ret, nghttp2_strerror(ret));

  lua_pushlstring(L, buf, ret);

  return 1;
}

static int hpack_decode(lua_State *L) {
  struct hpack_ctx_t *hctx = luaL_checkudata(L, 1, "__HPACK__");
  size_t bsize = 0;
  const char * buf = luaL_checklstring(L, 2, &bsize);
  if (!buf || bsize < 1)
    return luaL_error(L, "HPACK decode: Invalid string buffer.");

  lua_createtable(L, 32, 0);

  for (;;) {

    int inflate_flags = 0;

    nghttp2_nv nv = {};

    int dsize = nghttp2_hd_inflate_hd2(hctx->decoder, &nv, &inflate_flags, (uint8_t*)buf, bsize, 1);
    if (dsize < 0)
      return luaL_error(L, "HPACK decode failed with error: %s", nghttp2_strerror(dsize));

    /* 每次更新buffer位置 */
    buf += dsize; bsize -= dsize;

    if (inflate_flags & NGHTTP2_HD_INFLATE_FINAL){
      nghttp2_hd_inflate_end_headers(hctx->decoder);
      break;
    }

    if (inflate_flags & NGHTTP2_HD_INFLATE_EMIT) {
      lua_pushlstring(L, (const char*)nv.name, nv.namelen);
      lua_pushlstring(L, (const char*)nv.value, nv.valuelen);
      lua_rawset(L, -3);
    }

  }

  return 1;
}

static int hpack_gc(lua_State *L) {
  struct hpack_ctx_t *hctx = luaL_checkudata(L, 1, "__HPACK__");
  if (hctx->closed)
    return 1;
  hctx->closed = 1;
  nghttp2_hd_deflate_del(hctx->encoder);
  nghttp2_hd_inflate_del(hctx->decoder);
  return 1;
}

static int hpack_new(lua_State *L) {
  struct hpack_ctx_t *hctx = lua_newuserdata(L, sizeof(struct hpack_ctx_t));
  if (!hctx) return 0;

  hctx->tsize = luaL_checkinteger(L, 2);
  if (hctx->tsize <= 0)
    hctx->tsize = 4096;

  hctx->closed = 0;

  /* 初始化编码解码器 */
  int ret1, ret2;
  if ((ret1 = nghttp2_hd_deflate_new(&hctx->encoder, hctx->tsize)))
    return luaL_error(L, "HPACK internal error: %s", nghttp2_strerror(ret1));

  if ((ret2 = nghttp2_hd_inflate_new(&hctx->decoder)))
    return luaL_error(L, "HPACK internal error: %s", nghttp2_strerror(ret2));

  luaL_setmetatable(L, "__HPACK__");

  return 1;
}

static inline void hpack_init(lua_State *L) {
  luaL_newmetatable(L, "__HPACK__");
  /* index meta table */
  lua_pushstring (L, "__index");
  lua_pushvalue(L, -2);
  lua_rawset(L, -3);
  /* week table */
  lua_pushliteral(L, "__mode");
  lua_pushliteral(L, "kv");
  lua_rawset(L, -3);

  /* base method */
  luaL_Reg hpack_libs[] ={
    {"encode", hpack_encode},
    {"decode", hpack_decode},
    {"__gc",   hpack_gc},
    {NULL, NULL}
  };
  luaL_setfuncs(L, hpack_libs, 0);

  lua_newtable(L);
  /* new class */
  lua_pushliteral(L, "new");
  lua_pushcfunction(L, hpack_new);
  lua_rawset(L, -3);
  /* set version */
  lua_pushliteral(L, "http2_version");
  lua_pushliteral(L, NGHTTP2_VERSION);
  lua_rawset(L, -3);
}

LUAMOD_API int luaopen_lhpack(lua_State *L) {
  luaL_checkversion(L);
  hpack_init(L);
  return 1;
}


// // char buffer[4096] = {};

// // // SERVER
// // // char *src = "\x88\x76\x89\xaa\x63\x55\xe5\x80\xae\x17\xd7\x6b\x61\x96\xdf\x3d" \
// // //             "\xbf\x4a\x01\xb5\x34\x9f\xba\x82\x00\x80\xa0\x59\xb8\x17\xee\x01" \
// // //             "\xf5\x31\x68\xdf\x5f\x92\x49\x7c\xa5\x89\xd3\x4d\x1f\x6a\x12\x71" \
// // //             "\xd8\x82\xa6\x0b\x53\x2a\xcf\x7f\x5c\x03\x36\x31\x32\x6c\x96\xe4" \
// // //             "\x59\x3e\x94\x03\x4a\x69\x3f\x75\x04\x01\x01\x40\xb7\x70\x0d\x5c" \
// // //             "\x65\xd5\x31\x68\xdf\x00\x83\x2a\x47\x37\x8b\xfe\x5b\x94\x62\x23" \
// // //             "\x20\x6d\x61\x38\xd7\xf3\x00\x89\x19\x08\x5a\xd2\xb5\x83\xaa\x62" \
// // //             "\xa3\x84\x8f\xd2\x4a\x8f";

// // // CLIENT
// // // char *text = "\x82\x84\x86\x41\x86\xa0\xe4\x1d\x13\x9d\x09\x7a\x88\x25\xb6\x50\xc3\xab\xb8\xd2\xe1\x53\x03\x2a\x2f\x2a";

// // // char *rawK = ":authority";

// // // char *rawV = "localhost";

// // // unsigned char enTEXT[8] = {};
// // // char *enTEXT = "\x00\x87\x24\xab\x77\x2d\x88\x31\xea\x82\x0a\xe0";

// // char *enTEXT = "\x41\x86\xa0\xe4\x1d\x13\x9d\x09";

// // #define MAKE_NV(K, V)                                                          \
// //   {                                                                            \
// //     (uint8_t *)K, (uint8_t *)V, sizeof(K) - 1, sizeof(V) - 1,                  \
// //         NGHTTP2_NV_FLAG_NONE                                                   \
// //   }

// static void deflate(nghttp2_hd_deflater *deflater,
//                     nghttp2_hd_inflater *inflater, const nghttp2_nv *const nva,
//                     size_t nvlen);

// static int inflate_header_block(nghttp2_hd_inflater *inflater, uint8_t *in,
//                                 size_t inlen, int final);

// int main() {
//   int rv;
//   nghttp2_hd_deflater *deflater;
//   nghttp2_hd_inflater *inflater;
//   /* Define 1st header set.  This is looks like a HTTP request. */
//   nghttp2_nv nva1[] = {
//       MAKE_NV(":scheme", "https"), MAKE_NV(":authority", "example.org"),
//       MAKE_NV(":path", "/"), MAKE_NV("user-agent", "libnghttp2"),
//       MAKE_NV("accept-encoding", "gzip, deflate")};
//   /* Define 2nd header set */
//   nghttp2_nv nva2[] = {MAKE_NV(":scheme", "https"),
//                        MAKE_NV(":authority", "example.org"),
//                        MAKE_NV(":path", "/stylesheet/style.css"),
//                        MAKE_NV("user-agent", "libnghttp2"),
//                        MAKE_NV("accept-encoding", "gzip, deflate"),
//                        MAKE_NV("referer", "https://example.org")};

//   rv = nghttp2_hd_deflate_new(&deflater, 4096);

//   if (rv != 0) {
//     fprintf(stderr, "nghttp2_hd_deflate_init failed with error: %s\n",
//             nghttp2_strerror(rv));
//     exit(EXIT_FAILURE);
//   }

//   rv = nghttp2_hd_inflate_new(&inflater);

//   if (rv != 0) {
//     fprintf(stderr, "nghttp2_hd_inflate_init failed with error: %s\n",
//             nghttp2_strerror(rv));
//     exit(EXIT_FAILURE);
//   }

//   /* Encode and decode 1st header set */
//   deflate(deflater, inflater, nva1, sizeof(nva1) / sizeof(nva1[0]));

//   /* Encode and decode 2nd header set, using differential encoding
//      using state after encoding 1st header set. */
//   deflate(deflater, inflater, nva2, sizeof(nva2) / sizeof(nva2[0]));

//   nghttp2_hd_inflate_del(inflater);
//   nghttp2_hd_deflate_del(deflater);

//   return 0;
// }

// static void deflate(nghttp2_hd_deflater *deflater,
//                     nghttp2_hd_inflater *inflater, const nghttp2_nv *const nva,
//                     size_t nvlen) {
//   ssize_t rv;
//   uint8_t *buf;
//   size_t buflen;
//   size_t outlen;
//   size_t i;
//   size_t sum;

//   sum = 0;

//   for (i = 0; i < nvlen; ++i) {
//     sum += nva[i].namelen + nva[i].valuelen;
//   }

//   printf("Input (%zu byte(s)):\n\n", sum);

//   for (i = 0; i < nvlen; ++i) {
//     fwrite(nva[i].name, 1, nva[i].namelen, stdout);
//     printf(": ");
//     fwrite(nva[i].value, 1, nva[i].valuelen, stdout);
//     printf("\n");
//   }

//   buflen = nghttp2_hd_deflate_bound(deflater, nva, nvlen);
//   buf = malloc(buflen);

//   rv = nghttp2_hd_deflate_hd(deflater, buf, buflen, nva, nvlen);

//   if (rv < 0) {
//     fprintf(stderr, "nghttp2_hd_deflate_hd() failed with error: %s\n",
//             nghttp2_strerror((int)rv));

//     free(buf);

//     exit(EXIT_FAILURE);
//   }

//   outlen = (size_t)rv;

//   printf("\nDeflate (%zu byte(s), ratio %.02f):\n\n", outlen,
//          sum == 0 ? 0 : (double)outlen / (double)sum);

//   for (i = 0; i < outlen; ++i) {
//     if ((i & 0x0fu) == 0) {
//       printf("%08zX: ", i);
//     }

//     printf("%02X ", buf[i]);

//     if (((i + 1) & 0x0fu) == 0) {
//       printf("\n");
//     }
//   }

//   printf("\n\nInflate:\n\n");

//   /* We pass 1 to final parameter, because buf contains whole deflated
//      header data. */
//   rv = inflate_header_block(inflater, buf, outlen, 1);

//   if (rv != 0) {
//     free(buf);

//     exit(EXIT_FAILURE);
//   }

//   printf("\n-----------------------------------------------------------"
//          "--------------------\n");

//   free(buf);
// }

// int inflate_header_block(nghttp2_hd_inflater *inflater, uint8_t *in,
//                          size_t inlen, int final) {
//   ssize_t rv;

//   for (;;) {
//     nghttp2_nv nv;
//     int inflate_flags = 0;
//     size_t proclen;

//     rv = nghttp2_hd_inflate_hd(inflater, &nv, &inflate_flags, in, inlen, final);

//     if (rv < 0) {
//       fprintf(stderr, "inflate failed with error code %zd", rv);
//       return -1;
//     }

//     proclen = (size_t)rv;

//     in += proclen;
//     inlen -= proclen;

//     if (inflate_flags & NGHTTP2_HD_INFLATE_EMIT) {
//       fwrite(nv.name, 1, nv.namelen, stderr);
//       fprintf(stderr, ": ");
//       fwrite(nv.value, 1, nv.valuelen, stderr);
//       fprintf(stderr, "\n");
//     }

//     if (inflate_flags & NGHTTP2_HD_INFLATE_FINAL) {
//       nghttp2_hd_inflate_end_headers(inflater);
//       break;
//     }

//     if ((inflate_flags & NGHTTP2_HD_INFLATE_EMIT) == 0 && inlen == 0) {
//       break;
//     }
//   }

//   return 0;
// }