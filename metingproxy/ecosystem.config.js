module.exports = {
  apps: [
    {
      name: 'metingproxy',
      script: './dist/index.js',
      cwd: __dirname,

      // ---- 进程管理 ----
      instances: 1,
      exec_mode: 'fork',
      watch: false,
      max_memory_restart: '300M',
      autorestart: true,
      max_restarts: 10,
      restart_delay: 5000,
      kill_timeout: 10000,

      // ---- 日志 ----
      log_date_format: 'YYYY-MM-DD HH:mm:ss Z',
      merge_logs: true,
      out_file: './logs/pm2-out.log',
      error_file: './logs/pm2-error.log',

      // PM2 日志轮转: 安装 pm2-logrotate 后生效
      // pm2 install pm2-logrotate
      // pm2 set pm2-logrotate:max_size 20M
      // pm2 set pm2-logrotate:retain 5
      // pm2 set pm2-logrotate:compress true

      // ---- 环境变量 ----
      env: {
        NODE_ENV: 'production',
      },
    },
  ],
};
