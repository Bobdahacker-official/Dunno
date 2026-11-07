# ArticleNexus

A confusing personal joke project: ordinary browsers land in the RAM Appreciation Society, while recognised AI and search user agents receive ArticleNexus, a readable factual article library. The articles are also openly available at `/library` and `/articles/...` to everyone.

## Run

Requires Node.js 22 or later. No dependencies, API keys, analytics, or external assets.

```sh
cd ArticleNexus
npm start
# http://localhost:3000
npm test
npm run build
```

`PORT` defaults to 3000. The Node server binds to `0.0.0.0`. For a Node deployment behind a TLS proxy, set `PUBLIC_ORIGIN` to the public HTTPS origin (for example, your actual production domain) so canonical and sitemap URLs use HTTPS. The build also creates a Cloudflare Worker ES module at `dist/server/index.js`. To deploy to your Cloudflare account, use the Wrangler CLI with the included `wrangler.jsonc`; alternatively run `npm start` on a Node host. Mount this application at the origin root: its routes start with `/`.

## Included

- Responsive RAM fan page with original CSS illustrations and a three-chapter storyboard.
- Workload/capacity picker and a local-only, persistent appreciation toggle. No invented community counts.
- Six complete original explanatory articles, with links to authoritative sources.
- Article search, individual article routes, structured Article metadata, canonical URLs, Open Graph descriptions, robots.txt, and an XML sitemap.
- Web app manifest and SVG icon. This is not an offline PWA and does not install a service worker.
- Tests for crawler/browser routing, article bodies, metadata, caches, routes, and browser event handlers.

## Routing and indexing

The request handler uses a case-insensitive User-Agent allowlist for GPTBot, ChatGPT-User, OAI-SearchBot, Claude, Perplexity, Googlebot, Bing, and other named crawlers. Unrecognised or empty user agents receive the RAM view. This is a heuristic, not authentication: a user agent can be spoofed, and some AI browsers use ordinary browser agents. The server emits `Vary: User-Agent` and `Cache-Control: private, no-store` to prevent a shared cache from mixing the views. Keep those headers in production.

The root HTML title is **ArticleNexus**. Its description is:

> Search a curated library of factual articles on science, technology, nature, and our planet. Explore clear explanations, follow original sources, and discover the ideas behind everyday questions with ArticleNexus.

The project contains six articles; it does not claim a nonexistent 100,000-item collection. Google decides what title and snippet to display. Serving different content to crawlers and people is cloaking and can affect indexing or search eligibility. Publication and crawl access do not guarantee indexing. A public host is needed for search engines to access it; a private preview will not be indexed.

GitHub Pages alone cannot run request-based routing. Use a Node server or Worker host. URLs and the sitemap use the incoming request origin, so configure your production host/proxy to pass the correct public URL. All generated files are reproducible with `npm run build`.

## Verify both views

```sh
curl http://localhost:3000/
curl -A 'GPTBot/1.0' http://localhost:3000/
curl -A 'Googlebot' http://localhost:3000/
curl http://localhost:3000/articles/how-dram-remembers
```

Browser layout and rendering still require visual QA; DOM-adapter tests exercise handlers but do not replace a real browser. Sources are provided for further reading; no third-party article text is copied.
