/*
Copyright 2014 VirusTotal S.L. All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <getopt.h>


#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>


#include <jansson.h>


#include "VtFile.h"
#include "VtResponse.h"


#define DBG(FMT,ARG...) fprintf(stderr, "%s:%d: " FMT, __FUNCTION__, __LINE__, ##ARG);

void print_usage(const char *prog_name) {
  printf("%s < --apikey YOUR_API_KEY >   [ --filescan FILE1 ] [ --filescan FILE2 ]\n", prog_name);
  printf("    --apikey YOUR_API_KEY          Your virus total API key.  This arg 1st \n");
  printf("    --filescan FILE          File to scan.   Note may specify this multiple times for multiple files\n");
  printf("    --report SHA/MD5          Get a Report on a resource\n");
  printf("    --cluster YYYY-MM-DD          Get a Report on a resource\n");
  printf("    --download <hash>            Output file for download\n");
  printf("    --out <file>            Output file for download\n");
}

long long get_file_size(const char *path) {
  struct stat buf;
  int ret;


  ret = stat(path, &buf);
  if (ret == -1 ) {
    printf("Error: %s : %d : %m\n", path, errno);
    return -1;
  }
  return buf.st_size;
}


// Example data structure that can be passed to callback function
struct CallbackData {
  int counter;
};


void cluster_callback(json_t* cluster_json, void *data) {
  struct CallbackData *cb_data = (struct CallbackData *) data;
  char *s;

  cb_data->counter++;
  printf("------------- Result %d ----------------\n", cb_data->counter);

  s = json_dumps(cluster_json, JSON_INDENT(4));
  printf("%s \n", s);
  free(s);
  printf("\n");

}
#define RESP_BUF_SIZE 255

int main(int argc, char * const *argv) {
  int c;
  int ret = 0;
  struct VtFile *file_scan;
  struct VtResponse *response;
  char *str = NULL;
  char *api_key = NULL;
  char *out = NULL;
  int response_code;
  struct CallbackData cb_data = { .counter = 0 };
  char buf[RESP_BUF_SIZE+1] = { 0, };

  if (argc < 2) {
    print_usage(argv[0]);
    return 0;
  }

  file_scan = VtFile_new();

  while (1) {
    int option_index = 0;
    static struct option long_options[] = {
      {"filescan",  required_argument,    0,  'f' },
      {"rescan",  required_argument,    0,  'r' },
      {"report",  required_argument,    0,  'i' },
      {"apikey",  required_argument,     0,  'a'},
      {"clusters",  required_argument,     0,  'c'},
      {"download",  required_argument,     0,  'd'},
      {"out",  required_argument,     0,  'o'},
      {"verbose", optional_argument,  0,  'v' },
      {"help", optional_argument,  0,  'h' },
      {0,         0,                 0,  0 }
    };

    c = getopt_long_only(argc, argv, "",
                         long_options, &option_index);
    if (c == -1)
      break;

    switch (c) {
    case 'a':
      api_key = strdup(optarg);
      printf(" apikey: %s \n", optarg);
      VtFile_setApiKey(file_scan, optarg);
      break;
    case 'c':

      if (!api_key) {
        printf("Must set --apikey first\n");
        exit(1);
      }
      ret = VtFile_clusters(file_scan, optarg,
                            cluster_callback, &cb_data);
      DBG("Filescan clusters ret=%d\n", ret);
      if (ret) {
        printf("Error: %d \n", ret);
      }
      break;
    case 'd':
      if (!api_key) {
        printf("Must set --apikey first\n");
        exit(1);
      }
      if (!out) {
        printf("Must set --out first\n");
        exit(1);
      }
      ret = VtFile_downloadToFile(file_scan, optarg, out);
      DBG("Filescan download ret=%d\n", ret);
      if (ret) {
        printf("Error: %d \n", ret);
      }
      break;
    case 'f':
      if (!api_key) {
        printf("Must set --apikey first\n");
        exit(1);
      }

      ret = VtFile_scan(file_scan, optarg);
      DBG("Filescan ret=%d\n", ret);
      if (ret) {
        printf("Error: %d \n", ret);
      } else {
        response = VtFile_getResponse(file_scan);
        str = VtResponse_toJSONstr(response, VT_JSON_FLAG_INDENT);
        if (str) {
          printf("Response:\n%s\n", str);
          free(str);
        }
        VtResponse_put(&response);
      }
      break;
    case 'r':
      if (!api_key) {
        printf("Must set --apikey first\n");
        exit(1);
      }

      ret = VtFile_rescanHash(file_scan, optarg, 0, 0, 0, NULL, false);
      DBG("rescan ret=%d\n", ret);
      if (ret) {
        printf("Error: %d \n", ret);
      } else {
        response = VtFile_getResponse(file_scan);
        str = VtResponse_toJSONstr(response, VT_JSON_FLAG_INDENT);
        if (str) {
          printf("Response:\n%s\n", str);
          free(str);
        }
        VtResponse_put(&response);
      }
      break;
    case 'i':
      if (!api_key) {
        printf("Must set --apikey first\n");
        exit(1);
      }
      ret = VtFile_report(file_scan, optarg);
      DBG("rescan ret=%d\n", ret);
      if (ret) {
        printf("Error: %d \n", ret);
      } else {
        response = VtFile_getResponse(file_scan);
        str = VtResponse_toJSONstr(response, VT_JSON_FLAG_INDENT);
        if (str) {
          printf("Response:\n%s\n", str);
          free(str);
        }

        VtResponse_getVerboseMsg(response, buf, RESP_BUF_SIZE);
        printf("Msg: %s\n", buf);

        ret = VtResponse_getResponseCode(response, &response_code);
        if (!ret) {
          printf("response code: %d\n", response_code);
        }


        VtResponse_put(&response);
      }
      break;
    case 'o':

      if (out)
        free(out);

      out = strdup(optarg);

      break;
    case 'h':
      print_usage(argv[0]);
      goto cleanup;
    case 'v':
      printf(" verbose selected\n");
      if (optarg)
        printf(" verbose level %s \n", optarg);
      break;
    default:
      printf("?? getopt returned character code 0%o ??\n", c);
    }
  } // end while

  if (optind < argc) {
    printf("non-option ARGV-elements: ");
    while (optind < argc)
      printf("%s ", argv[optind++]);
    printf("\n");
  }
cleanup:
  DBG("Cleanup\n");
  VtFile_put(&file_scan);

  if (api_key)
    free(api_key);

  if (out)
    free(out);

  return 0;
}
