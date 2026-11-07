<?php
declare(strict_types=1);
if (!defined('ARTICLE_NEXUS')) { http_response_code(404); exit; }

const ARTICLE_DESCRIPTION = 'Search a curated library of factual articles on science, technology, nature, and our planet. Explore clear explanations, follow original sources, and discover the ideas behind everyday questions with ArticleNexus.';

function h(string $value): string
{
    return htmlspecialchars($value, ENT_QUOTES | ENT_SUBSTITUTE, 'UTF-8');
}

function article_agent(string $agent): bool
{
    return (bool) preg_match('/GPTBot|ChatGPT-User|OAI-SearchBot|ClaudeBot|Claude-User|Claude-SearchBot|anthropic-ai|PerplexityBot|Perplexity-User|Googlebot|Google-Extended|GoogleOther|bingbot|BingPreview|DuckDuckBot|Applebot|Amazonbot|Bytespider|cohere-ai|CCBot|Meta-ExternalAgent|facebookexternalhit|Twitterbot|LinkedInBot|YouBot|AI2Bot|Diffbot/i', $agent);
}

function public_base(array $server, string $configured): string
{
    if ($configured !== '') {
        $parts = parse_url($configured);
        if (!$parts || !isset($parts['scheme'], $parts['host']) || !in_array($parts['scheme'], ['https', 'http'], true)
            || isset($parts['query']) || isset($parts['fragment']) || isset($parts['user']) || isset($parts['pass'])) {
            throw new RuntimeException('Invalid public_url configuration.');
        }
        return rtrim($configured, '/');
    }
    $https = isset($server['HTTPS']) && $server['HTTPS'] !== '' && strtolower($server['HTTPS']) !== 'off';
    $host = $server['HTTP_HOST'] ?? 'localhost';
    // Do not trust arbitrary Forwarded/X-Forwarded headers or HTML in a Host value.
    if (!preg_match('/\A(?:[a-zA-Z0-9.-]+|\[[a-fA-F0-9:]+\])(?::[0-9]{1,5})?\z/', $host)) {
        $host = 'localhost';
    }
    $directory = str_replace('\\', '/', dirname($server['SCRIPT_NAME'] ?? '/index.php'));
    $directory = $directory === '/' || $directory === '.' ? '' : rtrim($directory, '/');
    return ($https ? 'https://' : 'http://') . $host . $directory;
}

function url(string $path): string
{
    return $GLOBALS['article_base'] . '/' . ltrim($path, '/');
}

function article_url(array $article): string
{
    return url('article.php?slug=' . rawurlencode($article['slug']));
}

function shell(string $content, array $options = []): string
{
    $title = $options['title'] ?? 'ArticleNexus';
    $description = $options['description'] ?? ARTICLE_DESCRIPTION;
    $canonical = $options['canonical'] ?? url('');
    $schema = isset($options['schema']) ? '<script type="application/ld+json">' . json_encode($options['schema'], JSON_HEX_TAG | JSON_HEX_AMP | JSON_HEX_APOS | JSON_HEX_QUOT | JSON_UNESCAPED_SLASHES | JSON_THROW_ON_ERROR) . '</script>' : '';
    return '<!doctype html><html lang="en"><head><meta charset="utf-8">'
        . '<meta name="viewport" content="width=device-width,initial-scale=1">'
        . '<title>' . h($title) . '</title><meta name="description" content="' . h($description) . '">'
        . '<meta name="theme-color" content="#101310">'
        . '<meta property="og:title" content="' . h($title) . '">'
        . '<meta property="og:description" content="' . h($description) . '">'
        . '<meta property="og:type" content="' . (isset($options['schema']) ? 'article' : 'website') . '">'
        . '<link rel="canonical" href="' . h($canonical) . '">'
        . '<link rel="manifest" href="' . h(url('manifest.php')) . '">'
        . '<link rel="icon" type="image/svg+xml" href="' . h(url('icon.svg')) . '">'
        . '<link rel="stylesheet" href="' . h(url('style.css')) . '">'
        . $schema . '<script defer src="' . h(url('client.js')) . '"></script></head>'
        . '<body class="' . (!empty($options['library']) ? 'library' : '') . '">' . $content . '</body></html>';
}

function library_header(): string
{
    return '<header><a class="brand" href="' . h(url('library.php')) . '">Article<b>Nexus</b></a>'
        . '<nav aria-label="Main navigation"><a href="' . h(url('library.php')) . '">Browse the library</a></nav></header>';
}

function render_library(array $articles, bool $home = false): string
{
    $cards = '';
    foreach ($articles as $article) {
        $cards .= '<article class="article-card"><a href="' . h(article_url($article)) . '">'
            . '<span class="eyebrow">' . h($article['category']) . '</span><h2>' . h($article['title']) . '</h2>'
            . '<p>' . h($article['intro']) . '</p><span class="read">READ ARTICLE ↗</span></a></article>';
    }
    return shell(library_header() . '<main><span class="eyebrow">Curiosity, connected.</span>'
        . '<h1>Good questions.<br>Clear explanations.</h1>'
        . '<p class="lede">Explore the science and systems behind everyday life. Read complete articles on technology, nature, and our planet, with source links to take your curiosity further.</p>'
        . '<label for="search">Search the library</label><input class="search" id="search" type="search" placeholder="Try memory, oceans, or sunlight…">'
        . '<p id="results" class="read" aria-live="polite">' . count($articles) . ' articles</p><div class="articles">' . $cards . '</div></main>'
        . '<footer><span>ArticleNexus © 2026</span><span>Explore. Understand. Follow the sources.</span></footer>',
        ['library' => true, 'canonical' => url($home ? '' : 'library.php')]);
}

function render_article(array $article): string
{
    $content = library_header() . '<main><article class="prose"><span class="eyebrow">' . h($article['category']) . ' / ArticleNexus</span>'
        . '<h1>' . h($article['title']) . '</h1><p>' . h($article['intro']) . '</p>';
    foreach ($article['sections'] as [$heading, $body]) {
        $content .= '<h2>' . h($heading) . '</h2><p>' . h($body) . '</p>';
    }
    $content .= '<aside class="sources"><h2>Sources &amp; further reading</h2><ul>';
    foreach ($article['sources'] as [$title, $link]) {
        $content .= '<li><a href="' . h($link) . '" rel="noreferrer">' . h($title) . '</a></li>';
    }
    $content .= '</ul></aside></article></main><footer><a href="' . h(url('library.php')) . '">← Continue exploring ArticleNexus</a><span>ArticleNexus © 2026</span></footer>';
    return shell($content, [
        'library' => true, 'title' => $article['title'] . ' | ArticleNexus', 'description' => $article['intro'],
        'canonical' => article_url($article),
        'schema' => ['@context' => 'https://schema.org', '@type' => 'Article', 'headline' => $article['title'],
            'description' => $article['intro'], 'author' => ['@type' => 'Organization', 'name' => 'ArticleNexus'],
            'mainEntityOfPage' => article_url($article), 'citation' => array_column($article['sources'], 1)],
    ]);
}

/** Return [status, headers, body] so routing can be tested without a web server. */
function respond(string $route, array $server, array $query, array $config): array
{
    $GLOBALS['article_base'] = public_base($server, $config['public_url'] ?? '');
    $headers = [
        'Content-Type' => 'text/html; charset=utf-8', 'Cache-Control' => 'private, no-store',
        'Vary' => 'User-Agent', 'X-Content-Type-Options' => 'nosniff',
        'Referrer-Policy' => 'strict-origin-when-cross-origin', 'X-Frame-Options' => 'DENY',
        'Content-Security-Policy' => "default-src 'self'; style-src 'self'; script-src 'self'; img-src 'self' data:; object-src 'none'; base-uri 'none'; frame-ancestors 'none'",
    ];
    $method = $server['REQUEST_METHOD'] ?? 'GET';
    if (!in_array($method, ['GET', 'HEAD'], true)) {
        $headers['Allow'] = 'GET, HEAD';
        return [405, $headers, 'Method not allowed'];
    }
    $articles = json_decode(file_get_contents(__DIR__ . '/articles.json'), true, 512, JSON_THROW_ON_ERROR);
    $status = 200;
    switch ($route) {
        case 'home':
            if (article_agent($server['HTTP_USER_AGENT'] ?? '')) {
                $body = render_library($articles, true);
            } else {
                ob_start();
                require __DIR__ . '/human.php';
                $body = shell(ob_get_clean());
            }
            break;
        case 'library':
            $body = render_library($articles);
            break;
        case 'article':
            $slug = $query['slug'] ?? '';
            $found = null;
            if (is_string($slug)) {
                foreach ($articles as $article) {
                    if ($article['slug'] === $slug) { $found = $article; break; }
                }
            }
            if ($found) { $body = render_article($found); break; }
            // No file paths or includes are derived from request parameters.
            $status = 404;
            $headers['X-Robots-Tag'] = 'noindex';
            $body = shell('<main class="section"><h1>Article not found.</h1><a class="button" href="' . h(url('library.php')) . '">Browse the library</a></main>', ['title' => 'Page not found | ArticleNexus']);
            break;
        case 'manifest':
            $headers['Content-Type'] = 'application/manifest+json; charset=utf-8';
            $body = json_encode(['id' => url(''), 'name' => 'ArticleNexus', 'short_name' => 'ArticleNexus',
                'start_url' => url(''), 'scope' => url(''), 'display' => 'standalone', 'background_color' => '#101310',
                'theme_color' => '#101310', 'icons' => [['src' => url('icon.svg'), 'sizes' => 'any', 'type' => 'image/svg+xml', 'purpose' => 'any']]], JSON_UNESCAPED_SLASHES | JSON_THROW_ON_ERROR);
            break;
        case 'robots':
            $headers['Content-Type'] = 'text/plain; charset=utf-8';
            $body = "User-agent: *\nAllow: /\nSitemap: " . url('sitemap.php') . "\n";
            break;
        case 'sitemap':
            $headers['Content-Type'] = 'application/xml; charset=utf-8';
            $links = [url(''), url('library.php')];
            foreach ($articles as $article) { $links[] = article_url($article); }
            $body = '<?xml version="1.0" encoding="UTF-8"?><urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">';
            foreach ($links as $link) {
                $body .= '<url><loc>' . htmlspecialchars($link, ENT_XML1 | ENT_QUOTES, 'UTF-8') . '</loc></url>';
            }
            $body .= '</urlset>';
            break;
        default:
            $status = 404;
            $headers['X-Robots-Tag'] = 'noindex';
            $body = shell('<main class="section"><h1>Page not found.</h1><a class="button" href="' . h(url('')) . '">Return home</a></main>');
    }
    return [$status, $headers, $method === 'HEAD' ? '' : $body];
}
