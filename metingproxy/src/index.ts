import express, { Request, Response, NextFunction } from 'express';
import axios from 'axios';
import http from 'http';
import fs from 'fs';
import path from 'path';
import yaml from 'js-yaml';
import winston from 'winston';

// ==================== 配置结构 ====================

interface AppConfig {
  server: { host: string; port: number };
  auth: { enabled: boolean; header_name: string; secret: string };
  meting: { base_url: string };
  cache: { ttl_minutes: number };
  logging: {
    level: string;
    file: { enabled: boolean; path: string; max_size_mb: number; max_files: number };
    console: { enabled: boolean; format: string };
  };
}

// ==================== 配置加载 ====================

function loadConfig(): AppConfig {
  const configPath = path.resolve(__dirname, '../config.yaml');
  if (!fs.existsSync(configPath)) {
    console.error('FATAL: config.yaml not found at', configPath);
    process.exit(1);
  }
  const raw = yaml.load(fs.readFileSync(configPath, 'utf8')) as Record<string, any>;

  return {
    server: { host: raw.server?.host ?? '0.0.0.0', port: raw.server?.port ?? 3000 },
    auth: {
      enabled: raw.auth?.enabled ?? false,
      header_name: raw.auth?.header_name ?? 'X-Auth-Key',
      secret: raw.auth?.secret ?? '',
    },
    meting: { base_url: raw.meting?.base_url ?? 'https://meting.xcnahida.cn/meting/api' },
    cache: { ttl_minutes: raw.cache?.ttl_minutes ?? 30 },
    logging: {
      level: raw.logging?.level ?? 'info',
      file: {
        enabled: raw.logging?.file?.enabled ?? true,
        path: raw.logging?.file?.path ?? './logs',
        max_size_mb: raw.logging?.file?.max_size_mb ?? 20,
        max_files: raw.logging?.file?.max_files ?? 5,
      },
      console: {
        enabled: raw.logging?.console?.enabled ?? true,
        format: raw.logging?.console?.format ?? 'simple',
      },
    },
  };
}

const CONFIG = loadConfig();

// ==================== 日志系统 ====================

function createLogger(config: AppConfig['logging']): winston.Logger {
  const logDir = path.resolve(__dirname, '..', config.file.path);
  if (config.file.enabled && !fs.existsSync(logDir)) {
    fs.mkdirSync(logDir, { recursive: true });
  }

  const transports: winston.transport[] = [];

  if (config.console.enabled) {
    const tpl = config.console.format === 'detailed'
      ? winston.format.combine(
          winston.format.timestamp({ format: 'YYYY-MM-DD HH:mm:ss' }),
          winston.format.colorize(),
          winston.format.printf(({ timestamp, level, message, module }) =>
            `[${timestamp}] ${level}${module ? ` [${module}]` : ''}: ${message}`
          ),
        )
      : winston.format.combine(
          winston.format.colorize(),
          winston.format.printf(({ level, message, module }) =>
            `${level}${module ? ` [${module}]` : ''}: ${message}`
          ),
        );
    transports.push(new winston.transports.Console({ format: tpl }));
  }

  if (config.file.enabled) {
    transports.push(new winston.transports.File({
      dirname: logDir,
      filename: 'metingproxy.log',
      maxsize: config.file.max_size_mb * 1024 * 1024,
      maxFiles: config.file.max_files,
      format: winston.format.combine(
        winston.format.timestamp({ format: 'YYYY-MM-DD HH:mm:ss.SSS' }),
        winston.format.printf(({ timestamp, level, message, module }) =>
          `[${timestamp}] ${level.toUpperCase()}${module ? ` [${module}]` : ''}: ${message}`
        ),
      ),
    }));
  }

  if (transports.length === 0) {
    transports.push(new winston.transports.Console());
  }

  return winston.createLogger({ level: config.level, transports });
}

const rootLogger = createLogger(CONFIG.logging);

function moduleLogger(module: string) {
  return {
    info: (msg: string) => rootLogger.info(msg, { module }),
    warn: (msg: string) => rootLogger.warn(msg, { module }),
    error: (msg: string) => rootLogger.error(msg, { module }),
    debug: (msg: string) => rootLogger.debug(msg, { module }),
  };
}

// ==================== 请求日志辅助 ====================

/**
 * 生成统一格式的请求摘要行
 * 
 * 客户端 IP 优先级:
 *   trust proxy 启用后 req.ip 自动从 X-Forwarded-For 头部取真实客户端 IP
 *   回退到 req.socket.remoteAddress (直连场景)
 * 
 * 格式: "GET /api/audio?id=xxx  <-  1.2.3.4  ->  meting?type=url&id=xxx"
 */
function reqSummary(req: Request, upstream: string): string {
  const route = `${req.method} ${req.originalUrl}`;
  // trust proxy 启用后 req.ip 已解析 X-Forwarded-For 头部
  const ip = req.ip || req.socket.remoteAddress || '-';
  return `${route}  <-  ${ip}  ->  ${upstream}`;
}

function formatBytes(bytes: number): string {
  if (bytes < 1024) return `${bytes}B`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)}KB`;
  return `${(bytes / (1024 * 1024)).toFixed(1)}MB`;
}

function formatDuration(ms: number): string {
  if (ms < 1000) return `${ms}ms`;
  if (ms < 60000) return `${(ms / 1000).toFixed(1)}s`;
  return `${(ms / 60000).toFixed(1)}min`;
}

// ==================== 常量 ====================

const METING_BASE = CONFIG.meting.base_url;
const PORT = CONFIG.server.port;
const HOST = CONFIG.server.host;
const CACHE_TTL_MS = CONFIG.cache.ttl_minutes * 60 * 1000;

// ==================== Express 初始化 ====================

const app = express();

// 信任反向代理头 (Nginx / CDN 透传真实客户端 IP)
// 支持 X-Forwarded-For, X-Forwarded-Proto, X-Real-IP, CF-Connecting-IP
app.set('trust proxy', true);

// ==================== 认证中间件 ====================

const authLogger = moduleLogger('auth');

function authMiddleware(req: Request, res: Response, next: NextFunction): void {
  if (!CONFIG.auth.enabled) {
    next();
    return;
  }

  if (req.path === '/health') {
    next();
    return;
  }

  const token = req.headers[CONFIG.auth.header_name.toLowerCase()] as string | undefined;
  if (!token || token !== CONFIG.auth.secret) {
    authLogger.warn(`AUTH_FAIL  ${req.method} ${req.originalUrl}  <-  ${req.ip}  |  reason: missing or invalid token`);
    res.status(401).json({ error: 'Unauthorized' });
    return;
  }

  next();
}

app.use(authMiddleware);

// ==================== 工具函数 ====================

async function resolveAudioUrl(songId: string): Promise<string> {
  const metingUrl = `${METING_BASE}?server=netease&type=url&id=${songId}`;

  const response = await axios.get(metingUrl, {
    maxRedirects: 0,
    validateStatus: (status) => status === 302 || status === 301 || status === 307 || status === 308,
    timeout: 10000,
  });

  const location = response.headers['location'];
  if (!location) {
    throw new Error('Meting API did not return a redirect location');
  }

  return location;
}

function extractSongId(url: string): string {
  if (!url) return '';
  const match = url.match(/[?&]id=(\d+)/);
  return match ? match[1] : '';
}

// ==================== 歌单缓存 ====================

interface CacheEntry {
  data: any;
  timestamp: number;
}
const playlistCache = new Map<string, CacheEntry>();

// ==================== API 路由 ====================

const plLogger = moduleLogger('playlist');
const audioLogger = moduleLogger('audio');
const lyricLogger = moduleLogger('lyric');
const rawLogger = moduleLogger('raw');

// ---- Playlist ----

app.get('/api/playlist', async (req: Request, res: Response) => {
  const playlistId = req.query.id as string;
  const upstream = `meting?type=playlist&id=${playlistId}`;
  const t0 = Date.now();

  if (!playlistId) {
    plLogger.warn(`${reqSummary(req, '-')}  |  400 Bad Request (missing id)`);
    res.status(400).json({ error: 'Missing playlist id parameter' });
    return;
  }

  // 检查缓存
  const cached = playlistCache.get(playlistId);
  if (cached && Date.now() - cached.timestamp < CACHE_TTL_MS) {
    const elapsed = Date.now() - t0;
    plLogger.info(`${reqSummary(req, upstream)}  |  CACHE HIT  |  ${cached.data.length} songs  |  ${formatDuration(elapsed)}`);
    res.json(cached.data);
    return;
  }

  try {
    plLogger.info(`${reqSummary(req, upstream)}  |  fetching...`);
    const metingUrl = `${METING_BASE}?server=netease&type=playlist&id=${playlistId}`;
    const response = await axios.get(metingUrl, { timeout: 30000 });
    const rawData = response.data;

    const simplified = Array.isArray(rawData)
      ? rawData.map((song: any) => {
          const songId = extractSongId(song.url);
            // 服务端已禁用 HTTPS，统一使用 HTTP
            return {
              title: song.title || song.name || 'Unknown Track',
              author: song.author || song.artist || '',
              pic: song.pic || '',
              id: songId,
              url: `http://${req.hostname}/api/audio?id=${songId}`,
              lrc: `http://${req.hostname}/api/lyric?id=${songId}`,
            };
        })
      : rawData;

    playlistCache.set(playlistId, { data: simplified, timestamp: Date.now() });

    const elapsed = Date.now() - t0;
    plLogger.info(`${reqSummary(req, upstream)}  |  200 OK  |  ${simplified.length} songs  |  cache updated  |  ${formatDuration(elapsed)}`);
    res.json(simplified);
  } catch (error: any) {
    const elapsed = Date.now() - t0;
    plLogger.error(`${reqSummary(req, upstream)}  |  502 FAIL  |  ${error.message}  |  ${formatDuration(elapsed)}`);
    res.status(502).json({ error: 'Failed to fetch playlist' });
  }
});

// ---- Audio ----

app.get('/api/audio', async (req: Request, res: Response) => {
  const songId = req.query.id as string;
  const upstream = `meting?type=url&id=${songId}`;
  const t0 = Date.now();

  if (!songId) {
    audioLogger.warn(`${reqSummary(req, '-')}  |  400 Bad Request (missing id)`);
    res.status(400).send('Missing song id parameter');
    return;
  }

  audioLogger.info(`${reqSummary(req, upstream)}  |  resolving redirect...`);

  try {
    const realAudioUrl = await resolveAudioUrl(songId);
    // 截取 CDN 域名和路径前部用于日志
    const cdnShort = realAudioUrl.replace(/^https?:\/\//, '').substring(0, 60);

    audioLogger.info(`${reqSummary(req, upstream)}  |  CDN -> ${cdnShort}...  |  streaming...`);

    const audioResponse = await axios.get(realAudioUrl, {
      responseType: 'stream',
      timeout: 0,
      headers: {
        'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36',
        'Referer': 'https://music.163.com/',
      },
    });

    const contentType = (audioResponse.headers['content-type'] as string) || '-';
    const contentLength = audioResponse.headers['content-length'] as string | undefined;

    if (contentType) res.setHeader('Content-Type', contentType);
    if (contentLength) res.setHeader('Content-Length', contentLength);
    res.setHeader('Accept-Ranges', 'bytes');

    let bytesSent = 0;
    audioResponse.data.on('data', (chunk: Buffer) => {
      bytesSent += chunk.length;
    });

    req.on('close', () => {
      const elapsed = Date.now() - t0;
      audioLogger.info(
        `${reqSummary(req, upstream)}  |  DISCONNECT  |  sent=${formatBytes(bytesSent)}  |  ${formatDuration(elapsed)}`
      );
      audioResponse.data.destroy();
    });

    audioResponse.data.pipe(res);

    audioResponse.data.on('end', () => {
      const elapsed = Date.now() - t0;
      audioLogger.info(
        `${reqSummary(req, upstream)}  |  DONE  |  type=${contentType}  |  sent=${formatBytes(bytesSent)}  |  ${formatDuration(elapsed)}`
      );
    });

    audioResponse.data.on('error', (err: Error) => {
      audioLogger.error(
        `${reqSummary(req, upstream)}  |  STREAM_ERR  |  ${err.message}  |  sent=${formatBytes(bytesSent)}`
      );
      if (!res.headersSent) {
        res.status(502).send('Audio stream error');
      } else {
        res.end();
      }
    });
  } catch (error: any) {
    const elapsed = Date.now() - t0;
    audioLogger.error(`${reqSummary(req, upstream)}  |  502 FAIL  |  ${error.message}  |  ${formatDuration(elapsed)}`);
    if (!res.headersSent) {
      res.status(502).send('Failed to fetch audio');
    }
  }
});

// ---- Lyric ----

app.get('/api/lyric', async (req: Request, res: Response) => {
  const songId = req.query.id as string;
  const upstream = `meting?type=lrc&id=${songId}`;
  const t0 = Date.now();

  if (!songId) {
    lyricLogger.warn(`${reqSummary(req, '-')}  |  400 Bad Request (missing id)`);
    res.status(400).json({ error: 'Missing song id parameter' });
    return;
  }

  try {
    lyricLogger.info(`${reqSummary(req, upstream)}  |  fetching...`);
    const metingUrl = `${METING_BASE}?server=netease&type=lrc&id=${songId}`;
    const response = await axios.get(metingUrl, {
      timeout: 10000,
      validateStatus: () => true, // 接收所有状态码 (纯音乐可能返回404/非200)
    });

    const data = response.data;
    // 检查是否包含歌词字段，日志标记无歌词情况
    const hasLyric = (data?.lrc?.lyric) || (data?.tlyric?.lyric) || data?.lyric;

    const elapsed = Date.now() - t0;
    if (hasLyric) {
      lyricLogger.info(`${reqSummary(req, upstream)}  |  ${response.status}  |  lyric found  |  ${formatDuration(elapsed)}`);
    } else {
      lyricLogger.info(`${reqSummary(req, upstream)}  |  ${response.status}  |  no lyric (instrumental)  |  ${formatDuration(elapsed)}`);
    }
    res.json(data);
  } catch (error: any) {
    const elapsed = Date.now() - t0;
    lyricLogger.error(`${reqSummary(req, upstream)}  |  502 FAIL  |  ${error.message}  |  ${formatDuration(elapsed)}`);
    res.status(502).json({ error: 'Failed to fetch lyrics' });
  }
});

// ---- Raw ----

app.get('/api/raw/*', async (req: Request, res: Response) => {
  const queryString = req.url.replace('/api/raw/', '').replace('/api/raw', '');
  const metingUrl = `${METING_BASE}${queryString}`;
  const upstream = `meting${queryString}`;
  const t0 = Date.now();

  rawLogger.info(`${reqSummary(req, upstream)}  |  passthrough...`);

  try {
    const response = await axios.get(metingUrl, {
      timeout: 30000,
      responseType: 'json',
      validateStatus: () => true,
    });

    const elapsed = Date.now() - t0;
    rawLogger.info(`${reqSummary(req, upstream)}  |  ${response.status}  |  ${formatDuration(elapsed)}`);

    res.status(response.status);
    const ct = response.headers['content-type'] as string | undefined;
    if (ct) res.setHeader('Content-Type', ct);
    res.send(response.data);
  } catch (error: any) {
    const elapsed = Date.now() - t0;
    rawLogger.error(`${reqSummary(req, upstream)}  |  502 FAIL  |  ${error.message}  |  ${formatDuration(elapsed)}`);
    res.status(502).json({ error: 'Failed to proxy request' });
  }
});

// ---- Health ----

app.get('/health', (_req: Request, res: Response) => {
  res.json({ status: 'ok', cache_size: playlistCache.size, uptime: process.uptime() });
});

// ==================== 启动服务 ====================

const server = http.createServer(app);

server.keepAliveTimeout = 120 * 1000;
server.headersTimeout = 125 * 1000;

server.listen(PORT, HOST, () => {
  const bootLogger = moduleLogger('boot');
  bootLogger.info('========================================');
  bootLogger.info(`  Meting Proxy Server v1.1.0`);
  bootLogger.info(`  Listening: http://${HOST}:${PORT}`);
  bootLogger.info(`  Auth: ${CONFIG.auth.enabled ? 'enabled' : 'disabled'}`);
  bootLogger.info(`  Log level: ${CONFIG.logging.level}`);
  bootLogger.info(`  Log file: ${CONFIG.logging.file.enabled ? (CONFIG.logging.file.path + '/metingproxy.log') : 'disabled'}`);
  bootLogger.info('========================================');
  bootLogger.info('Endpoints:');
  bootLogger.info('  GET /api/playlist?id=<id>     - playlist with cache');
  bootLogger.info('  GET /api/audio?id=<id>        - audio stream (live redirect)');
  bootLogger.info('  GET /api/lyric?id=<id>        - lyrics proxy');
  bootLogger.info('  GET /api/raw/*                - raw meting API passthrough');
  bootLogger.info('  GET /health                   - health check');
});
