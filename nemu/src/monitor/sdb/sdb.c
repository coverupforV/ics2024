/***************************************************************************************
* Copyright (c) 2014-2024 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include <inttypes.h>
#include <isa.h>
#include <cpu/cpu.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <utils.h>
#include <memory/vaddr.h>
#include "sdb.h"
#include <stdio.h>
#include <assert.h>

static int is_batch_mode = false;

void init_regex();
void init_wp_pool();

/* We use the `readline' library to provide more flexibility to read from stdin. */
static char* rl_gets() {
  static char *line_read = NULL;

  if (line_read) {
    free(line_read);
    line_read = NULL;
  }

  line_read = readline("(nemu) ");

  if (line_read && *line_read) {
    add_history(line_read);
  }

  return line_read;
}

static int cmd_c(char *args) {
  cpu_exec(-1);
  return 0;
}


static int cmd_q(char *args) {
  nemu_state.state = NEMU_QUIT;
  return -1;
}

static int cmd_si(char *args){
  char *num_str = strtok(NULL, " ");
  int num = 0;
  if (num_str != NULL) {
    assert("The input step num is not a number");
  } else {
    /* no argument given, the default num of step is 1 */
    num = 1;
  }
  cpu_exec(num);
  return 0;
}

static int cmd_info(char *args){
  char *arg = strtok(NULL, " ");
  if (strcmp(arg, "r") == 0) {
    /* info r : print the status of Rigister File*/
    isa_reg_display();
  } else if (strcmp(arg, "w") == 0) {
    /* info w : print the info of watch point*/
    display_wp();
  } else {
    printf("command info need argument r or w\n");
  }
  return 0;
}

static int cmd_x(char *args){
    unsigned int num = 1;
    vaddr_t addr;
    word_t mem;
    int ret;
    ret = sscanf(args, "%u %x", &num, &addr);
    if (ret < 2) {
        // 如果第一次读取失败，则尝试只读取十六进制数
        sscanf(args, "%x", &addr);
    }
    /*sscanf(args, "%u %x", &num, &addr);*/
    for (int i = 1; i <= num; i++) {
        if (i % 8 == 1) {
            printf("0x%08x :", addr);
        }
        mem = vaddr_read(addr, 1);
        printf("\t0x%02x", mem);
        if (i % 8 == 0 || i == num) {printf("\n"); };
        addr += 1;
    }
    return 0;
}

//表达式求值
static int cmd_p(char *args) {
    bool success = true;
    word_t res = expr(args, &success);
    if (success) {
        printf("%u\n", res);
    } else {
        printf("the expression is wrong, check it\n");
    }
    return 0;
}

static int cmd_w(char *args) {
    new_wp(args);
    return 0;
}

static int cmd_d(char *args) {
    int NO;
    sscanf(args, "%d", &NO);
    free_wp(NO);
    return 0;
}

static int cmd_help(char *args);

static struct {
  const char *name;
  const char *description;
  int (*handler) (char *);
} cmd_table [] = {
  { "help", "Display information about all supported commands", cmd_help },
  //{"test", "test pa1.2", cmd_test},
  { "c", "Continue the execution of the program", cmd_c },
  { "q", "Exit NEMU", cmd_q },
  { "si", "Let the program step into N instructions and then pause the execution, when N is not given, the default is 1",cmd_si},
  { "info", "Display information about regersters(r) or watchpoints(w)", cmd_info},
  { "x", "Calculate the value of the expression EXPR and use the result as the starting memory address",cmd_x},
  { "p", "expression evaluation", cmd_p},
  { "w", "Set Watchpoint", cmd_w},
  { "d", "Delete Watchpoint", cmd_d},

  /* TODO: Add more commands */

};

#define NR_CMD ARRLEN(cmd_table)

static int cmd_help(char *args) {
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  int i;

  if (arg == NULL) {
    /* no argument given */
    for (i = 0; i < NR_CMD; i ++) {
      printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  }
  else {
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(arg, cmd_table[i].name) == 0) {
        printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    printf("Unknown command '%s'\n", arg);
  }
  return 0;
}

void sdb_set_batch_mode() {
  is_batch_mode = true;
}

void sdb_mainloop() {
  if (is_batch_mode) {
    cmd_c(NULL);
    return;
  }

  for (char *str; (str = rl_gets()) != NULL; ) {
    char *str_end = str + strlen(str);

    /* extract the first token as the command */
    char *cmd = strtok(str, " ");
    if (cmd == NULL) { continue; }

    /* treat the remaining string as the arguments,
     * which may need further parsing
     */
    char *args = cmd + strlen(cmd) + 1;
    if (args >= str_end) {
      args = NULL;
    }

#ifdef CONFIG_DEVICE
    extern void sdl_clear_event_queue();
    sdl_clear_event_queue();
#endif

    int i;
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(cmd, cmd_table[i].name) == 0) {
        if (cmd_table[i].handler(args) < 0) { return; }
        break;
      }
    }

    if (i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
  }
}

void init_sdb() {
  /* Compile the regular expressions. */
  init_regex();

  /* Initialize the watchpoint pool. */
  init_wp_pool();
}
