# ArticleNexus — PHPVer

An independent PHP edition of ArticleNexus for ordinary shared hosting. The original Node.js project in the parent folder is unchanged.

PHP renders the same RAM Appreciation Society for ordinary visitors and the complete six-article ArticleNexus library for recognised AI/search user agents. Browser JavaScript handles the memory picker, appreciation toggle, and library search. No Node.js, npm, Composer, database, shell commands, sessions, background processes, or outbound requests are required on the host.

## Upload to InfinityFree

1. Create your hosting account/domain and open its File Manager, or connect with the FTP details shown in your account. Do not put FTP credentials in this repository.
2. Open that domain’s **htdocs** directory. Back up any existing website first.
3. Upload the **contents of `PHPVer/public/`** into `htdocs`, including both hidden `.htaccess` files and the `inc` directory. Upload extracted files, not the project ZIP itself. Keep `README.md` and `tests/` outside your public upload.
4. The entrypoint is `htdocs/index.php`. The included `DirectoryIndex` selects it. Preserve any hosting-provided configuration outside `htdocs`; merge custom rules carefully if you already have an application `.htaccess`.
5. Visit your domain in a real browser. You should see the RAM site. Visit `/library.php` for the article library and `/article.php?slug=how-dram-remembers` for an article.
6. Enable your hosting SSL certificate, then use HTTPS. If the canonical domain or HTTPS is detected incorrectly, edit `public/config.php` **before uploading it**: set `public_url` to your actual public origin, without a trailing slash. For a subfolder deployment, include the subfolder path. This file contains no credentials.

You can alternatively upload the same public contents into `htdocs/PHPVer/`. Navigation, assets, canonical links, the manifest scope, and sitemap automatically include `/PHPVer/`. The folder name is not hardcoded. PHP 8.0 or later is required; the implementation is tested on PHP 8.4.

## InfinityFree research (7 November 2026)

- The current [official hosting feature list](https://www.infinityfree.com/) advertises PHP 8.4, `.htaccess`, FTP, and online File Manager uploads. Older forum announcements mention PHP 8.3; the current feature page was used for the test version.
- The [official upload documentation](https://forum.infinityfree.com/t/why-are-my-files-deleted-after-uploading-them/49310) requires website files inside the domain’s `htdocs`. It lists 1 MB limits for PHP/HTML/JS and 10 kB for `.htaccess`. Every file here is well below those limits.
- The [mandatory browser security system](https://forum.infinityfree.com/t/browser-security-system-features-and-limitations/49353) can stop AI clients, command-line fetchers, link preview tools, manifests, and other automated clients **before PHP runs**. Changing a User-Agent cannot disable the hosting check. This application does not bypass it. The RAM visitor site is suitable for ordinary PHP hosting; reliable AI crawling requires a host that allows the intended clients. InfinityFree’s documentation says major search crawlers are compatible, but that does not guarantee this project will be indexed.

No live InfinityFree account was deployed or tested. These compatibility checks used PHP 8.4 and Apache locally in Docker.

## URLs and crawler behaviour

Direct endpoints work without URL rewriting:

| URL | Purpose |
| --- | --- |
| `/` or `/index.php` | RAM page for browsers; article library for recognised user agents |
| `/library.php` | Article library, accessible to everyone |
| `/article.php?slug=what-dns-does` | A complete article |
| `/manifest.php` | Web app manifest |
| `/robots.php` | Crawl directives and sitemap location |
| `/sitemap.php` | XML sitemap with the library and all six articles |

The included `.htaccess` also provides `/library`, `/articles/<slug>`, `/manifest.webmanifest`, `/robots.txt`, and `/sitemap.xml`. The application’s own links use the direct `.php` endpoints, so normal navigation does not depend on rewrites. Internal data/templates and direct requests to `config.php` are denied by Apache.

For a subfolder installation, search engines look for `robots.txt` at the **domain root**, not inside the subfolder. Add the subfolder sitemap URL to your existing domain-root robots.txt, or submit the full sitemap URL through your webmaster account. Do not overwrite another site’s root robots rules.

The request-based routing uses the same named-agent heuristic as the Node version. User agents are spoofable; unrecognised AI browsers may look like ordinary visitors. Both views emit `Vary: User-Agent` and `Cache-Control: private, no-store`. Preserve these headers in any cache or proxy you configure.

Metadata uses the ArticleNexus title and a factual expanded library description. There are six actual articles, with their sources. Google chooses its own displayed title/snippet. Different crawler and visitor content can affect search eligibility. The manifest is included, but this is not an offline PWA; InfinityFree’s security layer can also prevent manifest clients from fetching it.

## Local development and verification

With PHP installed, serve the public directory:

```sh
php -S 127.0.0.1:8000 -t public
php tests/run.php
```

The PHP development server supports direct `.php` endpoints; it does not process Apache `.htaccess` aliases. For Apache integration tests, from this folder with Docker installed:

```sh
docker run --rm --mount "type=bind,src=$PWD,dst=/app,readonly" -w /app php:8.4-apache php tests/run.php
docker run --rm --mount "type=bind,src=$PWD,dst=/app,readonly" php:8.4-apache sh /app/tests/apache.sh
```

Verification completed: PHP syntax checks, **337 PHP checks**, and **54 Apache HTTP checks**, covering root and subfolder uploads, all articles, normal and AI agents, hosting query parameters, assets, optional rewrites, manifests, sitemap, HEAD/POST, invalid slugs, and blocked internal files. The original Node tests cover the unchanged client controls. Real browser visual QA and a live InfinityFree upload remain unverified.
