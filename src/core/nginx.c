
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <nginx.h>


static ngx_int_t ngx_add_inherited_sockets(ngx_cycle_t *cycle);
static ngx_int_t ngx_get_options(int argc, char *const *argv);
static ngx_int_t ngx_process_options(ngx_cycle_t *cycle);
static ngx_int_t ngx_save_argv(ngx_cycle_t *cycle, int argc, char *const *argv);
static void *ngx_core_module_create_conf(ngx_cycle_t *cycle);
static char *ngx_core_module_init_conf(ngx_cycle_t *cycle, void *conf);
static char *ngx_set_user(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *ngx_set_env(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *ngx_set_priority(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *ngx_set_cpu_affinity(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_set_worker_processes(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);


static ngx_conf_enum_t  ngx_debug_points[] = {
    { ngx_string("stop"), NGX_DEBUG_POINTS_STOP },
    { ngx_string("abort"), NGX_DEBUG_POINTS_ABORT },
    { ngx_null_string, 0 }
};


// 定义了ngx_core_modules使用的配置指令，同时指定了指令的解析函数
// // 这些指令的type都是NGX_MAIN_CONF | NGX_DIRECT_CONF，只能出现在配置文件最外层的main域中
static ngx_command_t  ngx_core_commands[] = {

    { ngx_string("daemon"), // 守护进程指令
      NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot, //解析函数
      0,
      offsetof(ngx_core_conf_t, daemon),
      NULL },

    { ngx_string("master_process"), // master进程指令
      NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot, //简单地解析 on | off的函数
      0,
      offsetof(ngx_core_conf_t, master),
      NULL },

    { ngx_string("timer_resolution"),
      NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      0,
      offsetof(ngx_core_conf_t, timer_resolution),
      NULL },

    { ngx_string("pid"),
      NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      0,
      offsetof(ngx_core_conf_t, pid),
      NULL },

    { ngx_string("lock_file"),
      NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      0,
      offsetof(ngx_core_conf_t, lock_file),
      NULL },

    { ngx_string("worker_processes"),
      NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_TAKE1,
      ngx_set_worker_processes,
      0,
      0,
      NULL },

    { ngx_string("debug_points"),
      NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_enum_slot,
      0,
      offsetof(ngx_core_conf_t, debug_points),
      &ngx_debug_points },

    { ngx_string("user"),
      NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_TAKE12,
      ngx_set_user,
      0,
      0,
      NULL },

    { ngx_string("worker_priority"),
      NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_TAKE1,
      ngx_set_priority,
      0,
      0,
      NULL },

    { ngx_string("worker_cpu_affinity"),
      NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_1MORE,
      ngx_set_cpu_affinity,
      0,
      0,
      NULL },

    { ngx_string("worker_rlimit_nofile"),
      NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      0,
      offsetof(ngx_core_conf_t, rlimit_nofile),
      NULL },

    { ngx_string("worker_rlimit_core"),
      NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_off_slot,
      0,
      offsetof(ngx_core_conf_t, rlimit_core),
      NULL },

    { ngx_string("worker_rlimit_sigpending"),
      NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      0,
      offsetof(ngx_core_conf_t, rlimit_sigpending),
      NULL },

    { ngx_string("working_directory"),
      NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      0,
      offsetof(ngx_core_conf_t, working_directory),
      NULL },

    { ngx_string("env"),
      NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_TAKE1,
      ngx_set_env,
      0,
      0,
      NULL },

#if (NGX_OLD_THREADS)

    { ngx_string("worker_threads"),
      NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      0,
      offsetof(ngx_core_conf_t, worker_threads),
      NULL },

    { ngx_string("thread_stack_size"),
      NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      0,
      offsetof(ngx_core_conf_t, thread_stack_size),
      NULL },

#endif

      ngx_null_command  // 指令数组结束
};

/*
 * ngx_core_module.ctx，这里将creat_conf指向ngx_core_module_create_conf，
 * 将init_conf指向ngx_core_module_init_conf
 */
static ngx_core_module_t  ngx_core_module_ctx = {
    ngx_string("core"),             // 模块名字
    ngx_core_module_create_conf,    // 创建配置结构
    ngx_core_module_init_conf       // 初始化配置结构
};

/* 初始化ngx_core_module,这里将ctx指向 ngx_core_module_ctx (模块定义)*/
ngx_module_t  ngx_core_module = {
    NGX_MODULE_V1,                         /* 前7个字段 */
    &ngx_core_module_ctx,                  /* module context */ /* ngx_core_module_ctx */
    ngx_core_commands,                     /* module directives */ /* ngx_core_commands */
    NGX_CORE_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING                  /* 后8个字段 */
};


ngx_uint_t          ngx_max_module;

static ngx_uint_t   ngx_show_help;
static ngx_uint_t   ngx_show_version;
static ngx_uint_t   ngx_show_configure;
static u_char      *ngx_prefix;
static u_char      *ngx_conf_file;
static u_char      *ngx_conf_params;
static char        *ngx_signal;


static char **ngx_os_environ;

//1.时间、正则、错误日志、ssl等初始化
//2.读入命令行参数
//3.OS相关初始化
//4.读入并解析配置
//5.核心模块初始化
//6.创建各种临时文件和目录
//7.创建共享内存
//8.打开listen的端口
//9.所有模块初始化
//10.启动worker进程

/*
 * 主函数
*/
int ngx_cdecl //ngx_cdecl宏用于显式声明应使用的调用约定，在跨平台移植时有用，在linux版本的nginx程序中，该宏被定义为空
main(int argc, char *const *argv)
{
    ngx_int_t         i;
    ngx_log_t        *log;  			//保存日志结构
    ngx_cycle_t      *cycle, init_cycle; //初始化时的主结构体，在平滑升级服务器时，init_cycle负责升级前的信息,cycle负责升级后的信息
    ngx_core_conf_t  *ccf; 				//保存配置上下文

    ngx_debug_init();

    /* 初始化 ngx_sys_errlist */
	//ngx_strerror_init 用于初始化nginx服务器自定义的标准错误输出列表
    if (ngx_strerror_init() != NGX_OK) {
        return 1;
    }

    /* 获取参数，并设置对应的参数变量 */
	//获取参数和配置参数，比如命令是nginx -v 那么ngx_show_version就设置为1
    if (ngx_get_options(argc, argv) != NGX_OK) {
        return 1;
    }

    /*
     * 根据上一步设置的对应的参数变量进行相应的操作, ngx_show_version、ngx_show_help
     * 在 ngx_get_option() 中置 1
    */
    if (ngx_show_version) {
        ngx_write_stderr("nginx version: " NGINX_VER_BUILD NGX_LINEFEED);

        if (ngx_show_help) {
            ngx_write_stderr(
                "Usage: nginx [-?hvVtq] [-s signal] [-c filename] "
                             "[-p prefix] [-g directives]" NGX_LINEFEED
                             NGX_LINEFEED
                "Options:" NGX_LINEFEED
                "  -?,-h         : this help" NGX_LINEFEED
                "  -v            : show version and exit" NGX_LINEFEED
                "  -V            : show version and configure options then exit"
                                   NGX_LINEFEED
                "  -t            : test configuration and exit" NGX_LINEFEED
                "  -q            : suppress non-error messages "
                                   "during configuration testing" NGX_LINEFEED
                "  -s signal     : send signal to a master process: "
                                   "stop, quit, reopen, reload" NGX_LINEFEED
#ifdef NGX_PREFIX
                "  -p prefix     : set prefix path (default: "
                                   NGX_PREFIX ")" NGX_LINEFEED
#else
                "  -p prefix     : set prefix path (default: NONE)" NGX_LINEFEED
#endif
                "  -c filename   : set configuration file (default: "
                                   NGX_CONF_PATH ")" NGX_LINEFEED
                "  -g directives : set global directives out of configuration "
                                   "file" NGX_LINEFEED NGX_LINEFEED
                );
        }

        /* ngx_show_configure 在 ngx_get_option() 中置 1 */
        if (ngx_show_configure) {

#ifdef NGX_COMPILER
            ngx_write_stderr("built by " NGX_COMPILER NGX_LINEFEED);
#endif

#if (NGX_SSL)   //如果支持SSL
            if (SSLeay() == SSLEAY_VERSION_NUMBER) {
                ngx_write_stderr("built with " OPENSSL_VERSION_TEXT
                                 NGX_LINEFEED);
            } else {
                ngx_write_stderr("built with " OPENSSL_VERSION_TEXT
                                 " (running with ");
                ngx_write_stderr((char *) (uintptr_t)
                                 SSLeay_version(SSLEAY_VERSION));
                ngx_write_stderr(")" NGX_LINEFEED);
            }
#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
            ngx_write_stderr("TLS SNI support enabled" NGX_LINEFEED);
#else
            ngx_write_stderr("TLS SNI support disabled" NGX_LINEFEED);
#endif
#endif

            ngx_write_stderr("configure arguments:" NGX_CONFIGURE NGX_LINEFEED);
        }

        /* ngx_test_config 在 ngx_get_option() 中置 1 */
		//判断是否对nginx配置文件进行语法检查
        if (!ngx_test_config) {
            return 0;
        }
    }

    /* TODO */ ngx_max_sockets = -1;

    /*
     * 初始化各种系统时间,这里把ngx_cached_time指向cached_time[0],
     * 即第一个时间槽,然后调用ngx_time_update()更新时间槽
    */
    ngx_time_init();

#if (NGX_PCRE) //支持正则表达式
    ngx_regex_init();
#endif
    /* 获取master进程id */
    ngx_pid = ngx_getpid();

    /*
     * 初始化一个日志，这里是临时日志log，如果有ngx_prefix前缀则日志创建在该前缀路径中,
     * ngx_prefix 在 ngx_get_option() 中设置
     */
	// 初始化日志，如打开日志文件
    log = ngx_log_init(ngx_prefix);
    if (log == NULL) {
        return 1;
    }

    /* STUB */
#if (NGX_OPENSSL)
    ngx_ssl_init(log);/* 如果启用ssl则初始化ssl库 */
#endif

    /*
     * init_cycle->log is required for signal handlers and
     * ngx_process_options()
     */

    ngx_memzero(&init_cycle, sizeof(ngx_cycle_t));
    init_cycle.log = log; /* 把临时打开的日志赋给init_cycle（临时cycle） */
    ngx_cycle = &init_cycle; /* 主ngx_cycle指向临时cycle */

    /* 初始化init_cycle的pool内存池，大小为1024（这里只有ngx_pool_t.d被分配了空间） */
	/*内存管理初始化*/
    init_cycle.pool = ngx_create_pool(1024, log);
    if (init_cycle.pool == NULL) {
        return 1;
    }

    /*
     * 将argc argv,environ解析后保存在ngx_argc,ngx_os_argv（原始argv指针）,
     * ngx_argv（重新申请空间后将argv的值复制进ngx_argv）,ngx_os_environ中
	 * 保存参数到全局变量中
     */
    if (ngx_save_argv(&init_cycle, argc, argv) != NGX_OK) {
        return 1;
    }

    /*
     * 在init_cycle中保存:
     *      1.ngx_prefix(init_cycle.conf_prefix,init_cycle.prefix)，如果ngx_prefix
     *        为空，则为当前路径或NGX_PREFIX、NGX_CONF_PREFIX指定的路径（保存nginx主路径）
     *      2.ngx_conf_file(init_cycle.conf_file),如果ngx_conf_file未定义则使用
     *        NGX_CONF_PATH（nginx配置文件路径）
     *      3.ngx_conf_params(init_cycle.conf_param)（配置参数）
     *      4.ngx_test_config,设置日志等级（NGX_LOG_INFO）
	 *  初始化init_cycle中的一些如: conf_file，prefix，conf_prefix等字段
    */
    if (ngx_process_options(&init_cycle) != NGX_OK) {
        return 1;
    }

    /* 设置内存页、cache-line、cpu信息、最大socket数量等，并生成随机数种子*/
	// 初始化系统相关变量，如内存页面大小ngx_pagesize,ngx_cacheline_size,最大连接数ngx_max_sockets等
    if (ngx_os_init(log) != NGX_OK) {  // 这个ngx_os_init在不同操作系统调用不同的函数，根据系统参数来优化配置
        return 1;
    }

    /*
     * ngx_crc32_table_init() requires ngx_cacheline_size set in ngx_os_init()
     */
    /* 这里会用到上一步设置的ngx_cacheline_size，初始化CRC表，减少计算以加快速度*/
	// 初始化CRC表，提高效率，以后就不用计算了，直接用
    if (ngx_crc32_table_init() != NGX_OK) {
        return 1;
    }

    /* 
     * 继承socket，继承来的socket放在init_cycle.listening数组中（用于平滑升级，
     * 通过环境变量传递旧的监听套接字给新程序）
     */
	//继承的原因：在nginx服务器升级的情况下，保证web服务的平滑过渡，新的nginx能够继承旧nginx打开的socket
    if (ngx_add_inherited_sockets(&init_cycle) != NGX_OK) {
        return 1;
    }

    /* 计算module个数并为每个module设置索引值 */
	 // 计算模块个数，并且设置各个模块顺序（索引）
    ngx_max_module = 0;
    for (i = 0; ngx_modules[i]; i++) {
        ngx_modules[i]->index = ngx_max_module++;
    }

    /*
     * 初始化ngx_cycle_t并继承旧的ngx_cycle_t，解析配置文件，加载模块，
     * 打开所有需要监听的端口，初始化进程间通信方式，调用所有 core module 的 create_conf 
     * 和 init_conf 方法，调用各模块的init_module回调。（关键部分）
     */
	// 对ngx_cycle结构进行初始化,这里是nginx启动核心之处！
    cycle = ngx_init_cycle(&init_cycle);
    if (cycle == NULL) {
        if (ngx_test_config) {
            ngx_log_stderr(0, "configuration file %s test failed",
                           init_cycle.conf_file.data);
        }

        return 1;
    }

    /* ngx_test_config 在 ngx_get_option() 中设置 */
    if (ngx_test_config) {
        if (!ngx_quiet_mode) {
            ngx_log_stderr(0, "configuration file %s test is successful",
                           cycle->conf_file.data);
        }

        return 0;
    }

    /* ngx_signal 在 ngx_get_option() 中设置, -s 参数，向nginx进程发送对应的信号 */
	// 检查是否有设置信号处理，如有，进入ngx_signal_process处理
    if (ngx_signal) {
        return ngx_signal_process(cycle, ngx_signal);
    }

    /* 记录系统版本，编译信息等 */
    ngx_os_status(cycle->log);

    ngx_cycle = cycle;

    ccf = (ngx_core_conf_t *) ngx_get_conf(cycle->conf_ctx, ngx_core_module);

    /* 之前 ngx_process 未赋过值，所以全局变量 ngx_process 此时为 0 */
    if (ccf->master && ngx_process == NGX_PROCESS_SINGLE) {
        ngx_process = NGX_PROCESS_MASTER; /* 此时 ngx_process 被设为 1 */
    }

#if !(NGX_WIN32)

    /* 初始化信号处理（如当接收到CHCLD信号时设置 ngx_reap) */
    if (ngx_init_signals(cycle->log) != NGX_OK) {
        return 1;
    }

    /* 设置后台运行 */
	//如果是daemon模式，本进程变为守护进程
    if (!ngx_inherited && ccf->daemon) {
        if (ngx_daemon(cycle->log) != NGX_OK) {
            return 1;
        }

        ngx_daemonized = 1;
    }

    if (ngx_inherited) {
        ngx_daemonized = 1;
    }

#endif

    /* 创建pid文件 */
	// 创建进程记录文件
    if (ngx_create_pidfile(&ccf->pid, cycle->log) != NGX_OK) {
        return 1;
    }

    /* 重定向标准出错到日志文件 */
    if (ngx_log_redirect_stderr(cycle) != NGX_OK) {
        return 1;
    }

    if (log->file->fd != ngx_stderr) {
        if (ngx_close_file(log->file->fd) == NGX_FILE_ERROR) {
            ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno,
                          ngx_close_file_n " built-in log failed");
        }
    }

    ngx_use_stderr = 0;

    /* 进入单进程模式 */
	/*下面调用ngx_single_process_cycle或者ngx_master_process_cycle来启动nginx*/
    if (ngx_process == NGX_PROCESS_SINGLE) {
        ngx_single_process_cycle(cycle); //单进程，基本很少用single模式

        /* master worker 模式 */
    } else {
        /*
         * nginx 通过channel向子进程发指令，通过信号处理监控子进程状态
         * （如当子进程异常退出时 父进程会收到 SIGCHLD 信号，信号处理函数
         *  【在ngx_process.c文件中的ngx_signal_handler函数中处理】会设置
         *  ngx_reap 位为 1，在 master 循环中根据该标志位重启子进程）(在 Docker中
         *  通过管道读到 eof 作为信号通道同步父子进程状态)
         */
        ngx_master_process_cycle(cycle); //多进程，master进程进入这个，这个函数在不同操作系统有不同实现。
    }

    return 0;
}

/* 继承socket，平滑升级 */
static ngx_int_t
ngx_add_inherited_sockets(ngx_cycle_t *cycle)
{
    u_char           *p, *v, *inherited;
    ngx_int_t         s;
    ngx_listening_t  *ls;

    /* 获取环境变量 这里的"NGINX_VAR"是宏定义，值为"NGINX" */
	// 查看是否有设置NGINX这个环境变量
    inherited = (u_char *) getenv(NGINX_VAR);

    if (inherited == NULL) {
        return NGX_OK;
    }

    ngx_log_error(NGX_LOG_NOTICE, cycle->log, 0,
                  "using inherited sockets from \"%s\"", inherited);

    /* 初始化cycle.listening数组 */
    if (ngx_array_init(&cycle->listening, cycle->pool, 10,
                       sizeof(ngx_listening_t))
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    /* 遍历环境变量 */
    for (p = inherited, v = p; *p; p++) {
		//通过冒号或分号去除列表中的socket
        if (*p == ':' || *p == ';') {
            /* 转换socket */
            s = ngx_atoi(v, p - v);
            if (s == NGX_ERROR) {
                ngx_log_error(NGX_LOG_EMERG, cycle->log, 0,
                              "invalid socket number \"%s\" in " NGINX_VAR
                              " environment variable, ignoring the rest"
                              " of the variable", v);
                break;
            }

            v = p + 1;

            /* 从数组中取出一个可用元素 */
			//保存取出来的socket
            ls = ngx_array_push(&cycle->listening);
            if (ls == NULL) {
                return NGX_ERROR;
            }

            ngx_memzero(ls, sizeof(ngx_listening_t));

            ls->fd = (ngx_socket_t) s;
        }
    }

    /* 表示已经的得到要继承的socket */
	//标记继承工作的结束
    ngx_inherited = 1;

    /* 配置继承来的socket */
    return ngx_set_inherited_sockets(cycle);
}


char **
ngx_set_environment(ngx_cycle_t *cycle, ngx_uint_t *last)
{
    char             **p, **env;
    ngx_str_t         *var;
    ngx_uint_t         i, n;
    ngx_core_conf_t   *ccf;

    ccf = (ngx_core_conf_t *) ngx_get_conf(cycle->conf_ctx, ngx_core_module);

    if (last == NULL && ccf->environment) {
        return ccf->environment;
    }

    var = ccf->env.elts;

    for (i = 0; i < ccf->env.nelts; i++) {
        if (ngx_strcmp(var[i].data, "TZ") == 0
            || ngx_strncmp(var[i].data, "TZ=", 3) == 0)
        {
            goto tz_found;
        }
    }

    var = ngx_array_push(&ccf->env);
    if (var == NULL) {
        return NULL;
    }

    var->len = 2;
    var->data = (u_char *) "TZ";

    var = ccf->env.elts;

tz_found:

    n = 0;

    for (i = 0; i < ccf->env.nelts; i++) {

        if (var[i].data[var[i].len] == '=') {
            n++;
            continue;
        }

        for (p = ngx_os_environ; *p; p++) {

            if (ngx_strncmp(*p, var[i].data, var[i].len) == 0
                && (*p)[var[i].len] == '=')
            {
                n++;
                break;
            }
        }
    }

    if (last) {
        env = ngx_alloc((*last + n + 1) * sizeof(char *), cycle->log);
        *last = n;

    } else {
        env = ngx_palloc(cycle->pool, (n + 1) * sizeof(char *));
    }

    if (env == NULL) {
        return NULL;
    }

    n = 0;

    for (i = 0; i < ccf->env.nelts; i++) {

        if (var[i].data[var[i].len] == '=') {
            env[n++] = (char *) var[i].data;
            continue;
        }

        for (p = ngx_os_environ; *p; p++) {

            if (ngx_strncmp(*p, var[i].data, var[i].len) == 0
                && (*p)[var[i].len] == '=')
            {
                env[n++] = *p;
                break;
            }
        }
    }

    env[n] = NULL;

    if (last == NULL) {
        ccf->environment = env;
        environ = env;
    }

    return env;
}


ngx_pid_t
ngx_exec_new_binary(ngx_cycle_t *cycle, char *const *argv)
{
    char             **env, *var;
    u_char            *p;
    ngx_uint_t         i, n;
    ngx_pid_t          pid;
    ngx_exec_ctx_t     ctx;
    ngx_core_conf_t   *ccf;
    ngx_listening_t   *ls;

    ngx_memzero(&ctx, sizeof(ngx_exec_ctx_t));

    ctx.path = argv[0];
    ctx.name = "new binary process";
    ctx.argv = argv;

    n = 2;
    env = ngx_set_environment(cycle, &n);
    if (env == NULL) {
        return NGX_INVALID_PID;
    }

    var = ngx_alloc(sizeof(NGINX_VAR)
                    + cycle->listening.nelts * (NGX_INT32_LEN + 1) + 2,
                    cycle->log);
    if (var == NULL) {
        ngx_free(env);
        return NGX_INVALID_PID;
    }

    p = ngx_cpymem(var, NGINX_VAR "=", sizeof(NGINX_VAR));

    ls = cycle->listening.elts;
    for (i = 0; i < cycle->listening.nelts; i++) {
        p = ngx_sprintf(p, "%ud;", ls[i].fd);
    }

    *p = '\0';

    env[n++] = var;

#if (NGX_SETPROCTITLE_USES_ENV)

    /* allocate the spare 300 bytes for the new binary process title */

    env[n++] = "SPARE=XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"
               "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"
               "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"
               "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"
               "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";

#endif

    env[n] = NULL;

#if (NGX_DEBUG)
    {
    char  **e;
    for (e = env; *e; e++) {
        ngx_log_debug1(NGX_LOG_DEBUG_CORE, cycle->log, 0, "env: %s", *e);
    }
    }
#endif

    ctx.envp = (char *const *) env;

    ccf = (ngx_core_conf_t *) ngx_get_conf(cycle->conf_ctx, ngx_core_module);

    if (ngx_rename_file(ccf->pid.data, ccf->oldpid.data) == NGX_FILE_ERROR) {
        ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno,
                      ngx_rename_file_n " %s to %s failed "
                      "before executing new binary process \"%s\"",
                      ccf->pid.data, ccf->oldpid.data, argv[0]);

        ngx_free(env);
        ngx_free(var);

        return NGX_INVALID_PID;
    }

    pid = ngx_execute(cycle, &ctx);

    if (pid == NGX_INVALID_PID) {
        if (ngx_rename_file(ccf->oldpid.data, ccf->pid.data)
            == NGX_FILE_ERROR)
        {
            ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno,
                          ngx_rename_file_n " %s back to %s failed after "
                          "an attempt to execute new binary process \"%s\"",
                          ccf->oldpid.data, ccf->pid.data, argv[0]);
        }
    }

    ngx_free(env);
    ngx_free(var);

    return pid;
}

/*
 * 解析并获取系统参数，填充对应的选项参数
*/
/*get_options负责解析nginx启动时的命令参数*/
static ngx_int_t
ngx_get_options(int argc, char *const *argv)
{
    u_char     *p;
    ngx_int_t   i;

	//遍历参数
    for (i = 1; i < argc; i++) {

        p = (u_char *) argv[i];

        if (*p++ != '-') {
            ngx_log_stderr(0, "invalid option: \"%s\"", argv[i]);
            return NGX_ERROR;
        }

		//开始解析参数
        while (*p) {

            switch (*p++) {

            case '?':
            case 'h':
                ngx_show_version = 1;
                ngx_show_help = 1;
                break;

            case 'v':
                ngx_show_version = 1;
                break;

            case 'V':
                ngx_show_version = 1;
                ngx_show_configure = 1;
                break;

            case 't':
                ngx_test_config = 1;
                break;

            case 'q':
                ngx_quiet_mode = 1;
                break;

                /* 第一次给ngx_prefix赋值(程序安装目录) */
            case 'p':
                /* 如果-p后紧跟着前缀路径，调至next */
                if (*p) {
                    ngx_prefix = p;
                    goto next;
                }

                /* 否则-p参数空格后的字段为前缀路径 */
                if (argv[++i]) {
                    ngx_prefix = (u_char *) argv[i];
                    goto next;
                }

                /* 如果-p参数后没有跟路径 */
                ngx_log_stderr(0, "option \"-p\" requires directory name");
                return NGX_ERROR;

                /* 第一次给ngx_conf_file赋值(nginx.conf) */
            case 'c':
                /* 解析流程同ngx_prefix */
                if (*p) {
                    ngx_conf_file = p;
                    goto next;
                }

                if (argv[++i]) {
                    ngx_conf_file = (u_char *) argv[i];
                    goto next;
                }

                ngx_log_stderr(0, "option \"-c\" requires file name");
                return NGX_ERROR;

                /* 第一次给ngx_conf_params赋值(与配置相关的命令行参数) */
            case 'g':
                /* 解析流程同ngx_prefix */
                if (*p) {
                    ngx_conf_params = p;
                    goto next;
                }

                if (argv[++i]) {
                    ngx_conf_params = (u_char *) argv[i];
                    goto next;
                }

                ngx_log_stderr(0, "option \"-g\" requires parameter");
                return NGX_ERROR;

            case 's':
                if (*p) {
                    ngx_signal = (char *) p;

                } else if (argv[++i]) {
                    ngx_signal = argv[i];

                } else {
                    ngx_log_stderr(0, "option \"-s\" requires parameter");
                    return NGX_ERROR;
                }

                if (ngx_strcmp(ngx_signal, "stop") == 0
                    || ngx_strcmp(ngx_signal, "quit") == 0
                    || ngx_strcmp(ngx_signal, "reopen") == 0
                    || ngx_strcmp(ngx_signal, "reload") == 0)
                {
                    ngx_process = NGX_PROCESS_SIGNALLER;
                    goto next;
                }

                ngx_log_stderr(0, "invalid option: \"-s %s\"", ngx_signal);
                return NGX_ERROR;

            default:
                ngx_log_stderr(0, "invalid option: \"%c\"", *(p - 1));
                return NGX_ERROR;
            }
        }

    next:

        continue;
    }

    return NGX_OK;
}

/*
 * 解析处理保存参数
*/
static ngx_int_t
ngx_save_argv(ngx_cycle_t *cycle, int argc, char *const *argv)
{
#if (NGX_FREEBSD)

    ngx_os_argv = (char **) argv;
    ngx_argc = argc;
    ngx_argv = (char **) argv;

#else
    size_t     len;
    ngx_int_t  i;

    ngx_os_argv = (char **) argv;
    ngx_argc = argc;

    ngx_argv = ngx_alloc((argc + 1) * sizeof(char *), cycle->log);
    if (ngx_argv == NULL) {
        return NGX_ERROR;
    }

    for (i = 0; i < argc; i++) {
        len = ngx_strlen(argv[i]) + 1;

        ngx_argv[i] = ngx_alloc(len, cycle->log);
        if (ngx_argv[i] == NULL) {
            return NGX_ERROR;
        }

        (void) ngx_cpystrn((u_char *) ngx_argv[i], (u_char *) argv[i], len);
    }

    ngx_argv[i] = NULL;

#endif

	// 返回系统环境目录，如win7下为：c:\ProgramData
    ngx_os_environ = environ;

    return NGX_OK;
}

/*
 * 处理参数
*/
/*将nginx服务器启动时的参数保存到init_cycle结构的相应成员中*/
static ngx_int_t
ngx_process_options(ngx_cycle_t *cycle)
{
    u_char  *p;
    size_t   len;

    /* 如果指定ngx_prefix则conf_prefix和prefix使用ngx_prefix */
	//ngx_prefix 表示nginx安装路径
    if (ngx_prefix) {
        len = ngx_strlen(ngx_prefix);
        p = ngx_prefix;

        if (len && !ngx_path_separator(p[len - 1])) {
            p = ngx_pnalloc(cycle->pool, len + 1);
            if (p == NULL) {
                return NGX_ERROR;
            }

            ngx_memcpy(p, ngx_prefix, len);
            p[len++] = '/';
        }

        cycle->conf_prefix.len = len;
        cycle->conf_prefix.data = p;
        cycle->prefix.len = len;
        cycle->prefix.data = p;

    } else {

#ifndef NGX_PREFIX

        /* 如果没有指定ngx_prefix也没有宏定义NGX_PREFIX则使用当前工作目录 */
        p = ngx_pnalloc(cycle->pool, NGX_MAX_PATH);
        if (p == NULL) {
            return NGX_ERROR;
        }

        if (ngx_getcwd(p, NGX_MAX_PATH) == 0) {
            ngx_log_stderr(ngx_errno, "[emerg]: " ngx_getcwd_n " failed");
            return NGX_ERROR;
        }

        len = ngx_strlen(p);

        p[len++] = '/';

        cycle->conf_prefix.len = len;
        cycle->conf_prefix.data = p;
        cycle->prefix.len = len;
        cycle->prefix.data = p;

#else

#ifdef NGX_CONF_PREFIX
        ngx_str_set(&cycle->conf_prefix, NGX_CONF_PREFIX);
#else
        ngx_str_set(&cycle->conf_prefix, NGX_PREFIX);
#endif
        ngx_str_set(&cycle->prefix, NGX_PREFIX);

#endif
    }

    /* 如果指定了配置文件则直接使用，否则使用NGX_CONF_PATH宏定义 */
    if (ngx_conf_file) {
        cycle->conf_file.len = ngx_strlen(ngx_conf_file);
        cycle->conf_file.data = ngx_conf_file;

    } else {
        ngx_str_set(&cycle->conf_file, NGX_CONF_PATH);
    }

    /* 获取配置文件绝对路径，存在conf_file */
    if (ngx_conf_full_name(cycle, &cycle->conf_file, 0) != NGX_OK) {
        return NGX_ERROR;
    }

    /* 更新cycle.conf_prefix */
    for (p = cycle->conf_file.data + cycle->conf_file.len - 1;
         p > cycle->conf_file.data;
         p--)
    {
		// 将config_file路径按照分隔符“/”分割
        if (ngx_path_separator(*p)) {
            cycle->conf_prefix.len = p - ngx_cycle->conf_file.data + 1;
            cycle->conf_prefix.data = ngx_cycle->conf_file.data;
            break;
        }
    }

    if (ngx_conf_params) {
        cycle->conf_param.len = ngx_strlen(ngx_conf_params);
        cycle->conf_param.data = ngx_conf_params;
    }

    if (ngx_test_config) {
        cycle->log->log_level = NGX_LOG_INFO;
    }

    return NGX_OK;
}

/*
 * 为core module创建配置项,
 * 创建ngx_core_conf_t并进行初始化，
 * 用于解析配置前调用
 */
// 这是core模块对外的create_conf的钩子
static void *
ngx_core_module_create_conf(ngx_cycle_t *cycle)
{
	// 配置结构数据指针
    ngx_core_conf_t  *ccf;

	// 创建配置结构数据
    ccf = ngx_pcalloc(cycle->pool, sizeof(ngx_core_conf_t));
    if (ccf == NULL) {
        return NULL;
    }

    /*
     * set by ngx_pcalloc()
     *
     *     ccf->pid = NULL;
     *     ccf->oldpid = NULL;
     *     ccf->priority = 0;
     *     ccf->cpu_affinity_n = 0;
     *     ccf->cpu_affinity = NULL;
     */

    /* 初始化ngx_core_conf_t各字段 */
	// 只是简单初始化，设置一些默认值
    ccf->daemon = NGX_CONF_UNSET;
    ccf->master = NGX_CONF_UNSET;
    ccf->timer_resolution = NGX_CONF_UNSET_MSEC;

    ccf->worker_processes = NGX_CONF_UNSET;
    ccf->debug_points = NGX_CONF_UNSET;

    ccf->rlimit_nofile = NGX_CONF_UNSET;
    ccf->rlimit_core = NGX_CONF_UNSET;
    ccf->rlimit_sigpending = NGX_CONF_UNSET;

    ccf->user = (ngx_uid_t) NGX_CONF_UNSET_UINT;
    ccf->group = (ngx_gid_t) NGX_CONF_UNSET_UINT;

#if (NGX_OLD_THREADS)
    ccf->worker_threads = NGX_CONF_UNSET;
    ccf->thread_stack_size = NGX_CONF_UNSET_SIZE;
#endif

    if (ngx_array_init(&ccf->env, cycle->pool, 1, sizeof(ngx_str_t))
        != NGX_OK)
    {
        return NULL;
    }

    return ccf;
}

/*
 * 设置保存解析的配置项
*/
//初始化core模块的上下文结构
//参考：
//http://blog.csdn.net/livelylittlefish/article/details/7262750
static char *
ngx_core_module_init_conf(ngx_cycle_t *cycle, void *conf)
{
    ngx_core_conf_t  *ccf = conf;

    /* 设置各字段 */
	//下面的代码基本上是对conf结构体的成员进行初始化
    //初始化daemon、master等
    ngx_conf_init_value(ccf->daemon, 1); // 默认启用守护模式
    ngx_conf_init_value(ccf->master, 1); // 默认启动master进程
    ngx_conf_init_msec_value(ccf->timer_resolution, 0);

    ngx_conf_init_value(ccf->worker_processes, 1);
    ngx_conf_init_value(ccf->debug_points, 0);


//定义了nginx在多核CPU上的调度规则
#if (NGX_HAVE_CPU_AFFINITY)
    /* 设置cpu亲和性位 */
    if (ccf->cpu_affinity_n
        && ccf->cpu_affinity_n != 1
        && ccf->cpu_affinity_n != (ngx_uint_t) ccf->worker_processes)
    {
        ngx_log_error(NGX_LOG_WARN, cycle->log, 0,
                      "the number of \"worker_processes\" is not equal to "
                      "the number of \"worker_cpu_affinity\" masks, "
                      "using last mask for remaining worker processes");
    }

#endif

//多线程下的初始化
#if (NGX_OLD_THREADS)

    ngx_conf_init_value(ccf->worker_threads, 0);
    ngx_threads_n = ccf->worker_threads;
    ngx_conf_init_size_value(ccf->thread_stack_size, 2 * 1024 * 1024);

#endif


    /* 从core_conf中获取pid名并与cycle中prefix拼接 */
	//初始化pid、oldpid
    if (ccf->pid.len == 0) {
        ngx_str_set(&ccf->pid, NGX_PID_PATH);
    }

	//pan:判断nginx进程ID文件的路径是否完整
    if (ngx_conf_full_name(cycle, &ccf->pid, 0) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    ccf->oldpid.len = ccf->pid.len + sizeof(NGX_OLDPID_EXT);

    ccf->oldpid.data = ngx_pnalloc(cycle->pool, ccf->oldpid.len);
    if (ccf->oldpid.data == NULL) {
        return NGX_CONF_ERROR;
    }

    /* 将pid复制进oldpid并加后缀 */
	//更新旧进程id文件中的数据
    ngx_memcpy(ngx_cpymem(ccf->oldpid.data, ccf->pid.data, ccf->pid.len),
               NGX_OLDPID_EXT, sizeof(NGX_OLDPID_EXT));


#if !(NGX_WIN32)

    /* 如果配置中没有设置uid和gid，则使用NGX_USER和NGX_GROUP */
	 //初始化username,user,group
    if (ccf->user == (uid_t) NGX_CONF_UNSET_UINT && geteuid() == 0) {
        struct group   *grp;
        struct passwd  *pwd;

        ngx_set_errno(0);
        pwd = getpwnam(NGX_USER);
        if (pwd == NULL) {
            ngx_log_error(NGX_LOG_EMERG, cycle->log, ngx_errno,
                          "getpwnam(\"" NGX_USER "\") failed");
            return NGX_CONF_ERROR;
        }

        ccf->username = NGX_USER;
        ccf->user = pwd->pw_uid;

        ngx_set_errno(0);
        grp = getgrnam(NGX_GROUP);
        if (grp == NULL) {
            ngx_log_error(NGX_LOG_EMERG, cycle->log, ngx_errno,
                          "getgrnam(\"" NGX_GROUP "\") failed");
            return NGX_CONF_ERROR;
        }

        ccf->group = grp->gr_gid;
    }


    /* 未配置文件锁则使用默认 */
	 //初始化lock_file
    if (ccf->lock_file.len == 0) {
        ngx_str_set(&ccf->lock_file, NGX_LOCK_PATH);
    }

    /* 将prefix与锁文件名拼接 */
    if (ngx_conf_full_name(cycle, &ccf->lock_file, 0) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    {
    ngx_str_t  lock_file;

    lock_file = cycle->old_cycle->lock_file;

    /* 如果cycle中有继承来的锁文件（accept锁） */
    if (lock_file.len) {
        lock_file.len--;

        if (ccf->lock_file.len != lock_file.len
            || ngx_strncmp(ccf->lock_file.data, lock_file.data, lock_file.len)
               != 0)
        {
            ngx_log_error(NGX_LOG_EMERG, cycle->log, 0,
                          "\"lock_file\" could not be changed, ignored");
        }

        cycle->lock_file.len = lock_file.len + 1;
        lock_file.len += sizeof(".accept");

        cycle->lock_file.data = ngx_pstrdup(cycle->pool, &lock_file);
        if (cycle->lock_file.data == NULL) {
            return NGX_CONF_ERROR;
        }

    } else {
        cycle->lock_file.len = ccf->lock_file.len + 1;
        cycle->lock_file.data = ngx_pnalloc(cycle->pool,
                                      ccf->lock_file.len + sizeof(".accept"));
        if (cycle->lock_file.data == NULL) {
            return NGX_CONF_ERROR;
        }

        ngx_memcpy(ngx_cpymem(cycle->lock_file.data, ccf->lock_file.data,
                              ccf->lock_file.len),
                   ".accept", sizeof(".accept"));
    }
    }

#endif

    return NGX_CONF_OK;
}


static char *
ngx_set_user(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
#if (NGX_WIN32)

    ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                       "\"user\" is not supported, ignored");

    return NGX_CONF_OK;

#else

    ngx_core_conf_t  *ccf = conf;

    char             *group;
    struct passwd    *pwd;
    struct group     *grp;
    ngx_str_t        *value;

    if (ccf->user != (uid_t) NGX_CONF_UNSET_UINT) {
        return "is duplicate";
    }

    if (geteuid() != 0) {
        ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                           "the \"user\" directive makes sense only "
                           "if the master process runs "
                           "with super-user privileges, ignored");
        return NGX_CONF_OK;
    }

    value = (ngx_str_t *) cf->args->elts;

    ccf->username = (char *) value[1].data;

    ngx_set_errno(0);
    pwd = getpwnam((const char *) value[1].data);
    if (pwd == NULL) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, ngx_errno,
                           "getpwnam(\"%s\") failed", value[1].data);
        return NGX_CONF_ERROR;
    }

    ccf->user = pwd->pw_uid;

    group = (char *) ((cf->args->nelts == 2) ? value[1].data : value[2].data);

    ngx_set_errno(0);
    grp = getgrnam(group);
    if (grp == NULL) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, ngx_errno,
                           "getgrnam(\"%s\") failed", group);
        return NGX_CONF_ERROR;
    }

    ccf->group = grp->gr_gid;

    return NGX_CONF_OK;

#endif
}


static char *
ngx_set_env(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_core_conf_t  *ccf = conf;

    ngx_str_t   *value, *var;
    ngx_uint_t   i;

    var = ngx_array_push(&ccf->env);
    if (var == NULL) {
        return NGX_CONF_ERROR;
    }

    value = cf->args->elts;
    *var = value[1];

    for (i = 0; i < value[1].len; i++) {

        if (value[1].data[i] == '=') {

            var->len = i;

            return NGX_CONF_OK;
        }
    }

    return NGX_CONF_OK;
}


static char *
ngx_set_priority(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_core_conf_t  *ccf = conf;

    ngx_str_t        *value;
    ngx_uint_t        n, minus;

    if (ccf->priority != 0) {
        return "is duplicate";
    }

    value = cf->args->elts;

    if (value[1].data[0] == '-') {
        n = 1;
        minus = 1;

    } else if (value[1].data[0] == '+') {
        n = 1;
        minus = 0;

    } else {
        n = 0;
        minus = 0;
    }

    ccf->priority = ngx_atoi(&value[1].data[n], value[1].len - n);
    if (ccf->priority == NGX_ERROR) {
        return "invalid number";
    }

    if (minus) {
        ccf->priority = -ccf->priority;
    }

    return NGX_CONF_OK;
}


static char *
ngx_set_cpu_affinity(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
#if (NGX_HAVE_CPU_AFFINITY)
    ngx_core_conf_t  *ccf = conf;

    u_char            ch;
    uint64_t         *mask;
    ngx_str_t        *value;
    ngx_uint_t        i, n;

    if (ccf->cpu_affinity) {
        return "is duplicate";
    }

    mask = ngx_palloc(cf->pool, (cf->args->nelts - 1) * sizeof(uint64_t));
    if (mask == NULL) {
        return NGX_CONF_ERROR;
    }

    ccf->cpu_affinity_n = cf->args->nelts - 1;
    ccf->cpu_affinity = mask;

    value = cf->args->elts;

    for (n = 1; n < cf->args->nelts; n++) {

        if (value[n].len > 64) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                         "\"worker_cpu_affinity\" supports up to 64 CPUs only");
            return NGX_CONF_ERROR;
        }

        mask[n - 1] = 0;

        for (i = 0; i < value[n].len; i++) {

            ch = value[n].data[i];

            if (ch == ' ') {
                continue;
            }

            mask[n - 1] <<= 1;

            if (ch == '0') {
                continue;
            }

            if (ch == '1') {
                mask[n - 1] |= 1;
                continue;
            }

            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                          "invalid character \"%c\" in \"worker_cpu_affinity\"",
                          ch);
            return NGX_CONF_ERROR;
        }
    }

#else

    ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                       "\"worker_cpu_affinity\" is not supported "
                       "on this platform, ignored");
#endif

    return NGX_CONF_OK;
}


uint64_t
ngx_get_cpu_affinity(ngx_uint_t n)
{
    ngx_core_conf_t  *ccf;

    ccf = (ngx_core_conf_t *) ngx_get_conf(ngx_cycle->conf_ctx,
                                           ngx_core_module);

    if (ccf->cpu_affinity == NULL) {
        return 0;
    }

    if (ccf->cpu_affinity_n > n) {
        return ccf->cpu_affinity[n];
    }

    return ccf->cpu_affinity[ccf->cpu_affinity_n - 1];
}


static char *
ngx_set_worker_processes(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_str_t        *value;
    ngx_core_conf_t  *ccf;

    ccf = (ngx_core_conf_t *) conf;

    if (ccf->worker_processes != NGX_CONF_UNSET) {
        return "is duplicate";
    }

    value = (ngx_str_t *) cf->args->elts;

    if (ngx_strcmp(value[1].data, "auto") == 0) {
        ccf->worker_processes = ngx_ncpu;
        return NGX_CONF_OK;
    }

    ccf->worker_processes = ngx_atoi(value[1].data, value[1].len);

    if (ccf->worker_processes == NGX_ERROR) {
        return "invalid value";
    }

    return NGX_CONF_OK;
}
