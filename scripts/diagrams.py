"""Architecture figures for the SAVC paper, generated as SVG and rasterised with headless Chromium."""
import os, asyncio
from xml.sax.saxutils import escape

OUT = 'figures'
os.makedirs(OUT, exist_ok=True)
FONT = "'Liberation Sans', Arial, Helvetica, sans-serif"

INK = '#1f1f1e'
INK2 = '#52514e'
LINE = '#3b3b39'
C = {
    'savc': ('#eef4fc', '#2a78d6', '#1c5cab'),
    'venue': ('#eaf6f0', '#1b9a6c', '#126b4b'),
    'author': ('#fdf1ea', '#e0703f', '#a8461c'),
    'ledger': ('#f1effc', '#6250d6', '#45389e'),
    'attack': ('#fdeeee', '#d64545', '#b02c2c'),
    'gray': ('#f5f4f1', '#8a8984', '#3b3b39'),
    'white': ('#ffffff', '#8a8984', '#3b3b39'),
}


class Svg:
    def __init__(self, w, h):
        self.w, self.h, self.items = w, h, []

    def add(self, s):
        self.items.append(s)

    def text(self, x, y, s, size=13, weight='normal', color=INK, anchor='middle', italic=False):
        st = 'italic' if italic else 'normal'
        self.add(f'<text x="{x}" y="{y}" font-size="{size}" font-weight="{weight}" font-style="{st}" fill="{color}" '
                 f'text-anchor="{anchor}" dominant-baseline="middle">{escape(s)}</text>')

    def lines(self, x, y, arr, size=12, lh=None, color=INK2, anchor='middle', weight='normal'):
        lh = lh or size * 1.32
        y0 = y - (len(arr) - 1) * lh / 2
        for i, s in enumerate(arr):
            self.text(x, y0 + i * lh, s, size, weight, color, anchor)

    def rect(self, x, y, w, h, kind='gray', r=8, dash=None, sw=1.4, fill=None):
        f, s, _ = C[kind]
        d = f' stroke-dasharray="{dash}"' if dash else ''
        self.add(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{r}" fill="{fill or f}" stroke="{s}" stroke-width="{sw}"{d}/>')

    def box(self, x, y, w, h, title, body=(), kind='gray', tsize=13.5, bsize=11.5, fill=None, dash=None, title_dy=0):
        self.rect(x, y, w, h, kind, fill=fill, dash=dash)
        tc = C[kind][2]
        if body:
            n = len(body)
            block = 18 + n * bsize * 1.32
            top = y + (h - block) / 2
            self.text(x + w / 2, top + 8 + title_dy, title, tsize, 'bold', tc)
            self.lines(x + w / 2, top + 20 + n * bsize * 1.32 / 2 + 2, list(body), bsize)
        else:
            self.text(x + w / 2, y + h / 2, title, tsize, 'bold', tc)

    def header(self, x, y, w, label, kind):
        _, s, t = C[kind]
        self.text(x + 14, y + 18, label, 13, 'bold', t, anchor='start')

    def arrow(self, pts, color=LINE, dash=None, sw=1.5, head=True, both=False, label=None, lpos=None, lsize=11,
              lcolor=INK2, lanchor='middle', lbg=True):
        d = 'M ' + ' L '.join(f'{x},{y}' for x, y in pts)
        mid = {LINE: 'a', C['attack'][1]: 'r', C['ledger'][1]: 'v', C['savc'][1]: 'b', C['venue'][1]: 'g', C['author'][1]: 'o'}.get(color, 'a')
        m = f' marker-end="url(#h{mid})"' if head else ''
        if both:
            m += f' marker-start="url(#t{mid})"'
        da = f' stroke-dasharray="{dash}"' if dash else ''
        self.add(f'<path d="{d}" fill="none" stroke="{color}" stroke-width="{sw}"{da}{m}/>')
        if label:
            lx, ly = lpos
            if isinstance(label, str):
                label = [label]
            if lbg:
                wmax = max(len(s) for s in label) * lsize * 0.53 + 10
                hh = len(label) * lsize * 1.3 + 4
                ox = {'middle': wmax / 2, 'start': 5, 'end': wmax - 5}[lanchor]
                self.add(f'<rect x="{lx - ox}" y="{ly - hh / 2}" width="{wmax}" height="{hh}" fill="white" opacity="0.92"/>')
            self.lines(lx, ly, label, lsize, color=lcolor, anchor=lanchor)

    def badge(self, x, y, n, kind='savc', r=10):
        _, s, _ = C[kind]
        self.add(f'<circle cx="{x}" cy="{y}" r="{r}" fill="{s}"/>')
        self.text(x, y + 0.5, str(n), 11.5, 'bold', 'white')

    def cross(self, x, y, r=9):
        c = C['attack'][1]
        self.add(f'<circle cx="{x}" cy="{y}" r="{r + 3}" fill="white" stroke="{c}" stroke-width="1.6"/>')
        self.add(f'<path d="M{x - r / 2},{y - r / 2} L{x + r / 2},{y + r / 2} M{x + r / 2},{y - r / 2} L{x - r / 2},{y + r / 2}" stroke="{c}" stroke-width="2.2"/>')

    def check(self, x, y, r=9):
        c = C['venue'][1]
        self.add(f'<circle cx="{x}" cy="{y}" r="{r + 3}" fill="white" stroke="{c}" stroke-width="1.6"/>')
        self.add(f'<path d="M{x - r / 2},{y} L{x - r / 8},{y + r / 2.2} L{x + r / 1.8},{y - r / 2.2}" fill="none" stroke="{c}" stroke-width="2.2"/>')

    def svg(self):
        defs = []
        for k, col in [('a', LINE), ('r', C['attack'][1]), ('v', C['ledger'][1]), ('b', C['savc'][1]),
                       ('g', C['venue'][1]), ('o', C['author'][1])]:
            defs.append(f'<marker id="h{k}" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse">'
                        f'<path d="M0,0 L10,5 L0,10 z" fill="{col}"/></marker>')
            defs.append(f'<marker id="t{k}" viewBox="0 0 10 10" refX="1" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse">'
                        f'<path d="M0,0 L10,5 L0,10 z" fill="{col}"/></marker>')
        return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{self.w}" height="{self.h}" viewBox="0 0 {self.w} {self.h}" '
                f'font-family="{FONT}"><defs>{"".join(defs)}</defs><rect width="100%" height="100%" fill="white"/>'
                + ''.join(self.items) + '</svg>')


# ============================================================ Fig 1: architecture and trust model
def fig_architecture():
    s = Svg(1400, 860)
    # Attacker (top)
    s.box(560, 18, 280, 72, 'Attacker', ['look-alike domains, cloned venue websites,', 'compromised mailboxes'], 'attack', dash='6 4')
    # Venues
    s.rect(30, 150, 290, 470, 'venue', r=12)
    s.header(30, 150, 290, 'Registered venues', 'venue')
    s.text(44, 192, 'Journals and conferences', 11.5, color=INK2, anchor='start')
    s.box(55, 220, 240, 110, 'Venue profile', ['ISSN / organizing body', 'official domain', 'verified payment accounts'], 'white')
    s.box(55, 350, 240, 110, 'Authorized representatives', ['editors, organizers', 'individual credentials', 'signing keys'], 'white')
    s.box(55, 480, 240, 110, 'Outgoing communication', ['posted on SAVC (Full Mediation)', 'or registered before email', '(Verification Bridge)'], 'white')
    # Authors
    s.rect(1080, 150, 290, 470, 'author', r=12)
    s.header(1080, 150, 290, 'Authors', 'author')
    s.text(1094, 192, 'Researchers submitting or registering', 11.5, color=INK2, anchor='start')
    s.box(1105, 220, 240, 110, 'Email inbox', ['content-free notifications', 'or messages carrying a', 'VVC reference'], 'white')
    s.box(1105, 350, 240, 110, 'Known-address access', ['bookmarked portal, app,', 'or browser extension;', 'never links in messages'], 'white')
    s.box(1105, 480, 240, 110, 'Decision', ['trust only what verifies;', 'pay only registered', 'fee requests'], 'white')
    # SAVC intermediary
    s.rect(390, 150, 620, 470, 'savc', r=12, sw=1.8)
    s.header(390, 150, 620, 'SAVC independent intermediary (neutral, non-profit operator)', 'savc')
    mods = [
        ('Venue Registry & Governance', ['vetting, renewal,', 'suspension, revocation']),
        ('VVC Issuance', ['reference + HMAC/signature', 'bound to recipient & content']),
        ('Secure Messaging', ['signed in-platform messages,', 'content-free notifications']),
        ('Verification Engine', ['registry lookup, digest,', 'expiry, key status']),
        ('Payment Verification', ['registered accounts', 'and fee requests']),
        ('Access & Audit API', ['author portal, extension,', 'OJS / CMS plugins']),
    ]
    for i, (t, b) in enumerate(mods):
        cx, cy = 410 + (i % 2) * 300, 190 + (i // 2) * 140
        s.box(cx, cy, 280, 118, t, b, 'white')
    # Ledger
    s.rect(390, 690, 620, 150, 'ledger', r=12)
    s.header(390, 690, 620, 'Verification ledger (append-only, multi-party)', 'ledger')
    nodes = ['Intermediary', 'Publishers', 'Indexing bodies', 'Academic orgs']
    for i, n in enumerate(nodes):
        x = 470 + i * 155
        s.add(f'<rect x="{x - 60}" y="740" width="120" height="44" rx="22" fill="white" stroke="{C["ledger"][1]}" stroke-width="1.4"/>')
        s.text(x, 762, n, 11.5, 'bold', C['ledger'][2])
        if i < 3:
            s.arrow([(x + 62, 762), (x + 93, 762)], C['ledger'][1], head=True, both=True, sw=1.2)
    s.text(700, 812, 'venue keys · key rotations and revocations · payment accounts · VVC issuance records', 11.5, color=INK2)
    # arrows venue -> savc
    s.arrow([(322, 270), (388, 270)], C['venue'][1], label=['register', '& vet'], lpos=(355, 245), lsize=10.5)
    s.arrow([(322, 535), (388, 535)], C['venue'][1], label=['post /', 'register'], lpos=(355, 510), lsize=10.5)
    # savc -> author
    s.arrow([(1012, 270), (1078, 270)], C['savc'][1], label=['notify'], lpos=(1045, 252), lsize=10.5)
    s.arrow([(1078, 405), (1012, 405)], C['author'][1], label=['verify /', 'read'], lpos=(1045, 380), lsize=10.5)
    s.arrow([(1012, 535), (1078, 535)], C['savc'][1], label=['result'], lpos=(1045, 517), lsize=10.5)
    # savc <-> ledger
    s.arrow([(700, 622), (700, 688)], C['ledger'][1], both=True, label=['anchor records / check key status'], lpos=(700, 655), lsize=10.5)
    # email channel venue -> author (top, dashed)
    s.arrow([(175, 148), (175, 118), (1180, 118), (1180, 148)], INK2, dash='5 4', sw=1.3,
            label='email channel (untrusted carrier)', lpos=(330, 118), lsize=10.5)
    # attacker arrows
    s.arrow([(840, 54), (1300, 54), (1300, 148)], C['attack'][1], dash='6 4', sw=1.5,
            label=['spoofed / cloned messages'], lpos=(1060, 54), lsize=10.5, lcolor=C['attack'][2])
    s.arrow([(700, 92), (700, 148)], C['attack'][1], dash='6 4', sw=1.5)
    s.cross(700, 132)
    s.text(718, 140, 'cannot post or obtain valid VVCs', 10.5, color=C['attack'][2], anchor='start')
    return s


# ============================================================ Fig 2: dual-mode sequence diagrams
def seq_panel(s, x0, title, lanes, steps, height):
    s.text(x0 + 8, 30, title, 14.5, 'bold', INK, anchor='start')
    n = len(lanes)
    w = 640
    xs = [x0 + 70 + i * (w - 140) / (n - 1) for i in range(n)]
    for (name, kind), x in zip(lanes, xs):
        s.box(x - 72, 52, 144, 50, name, (), kind, tsize=12.5)
        s.add(f'<line x1="{x}" y1="104" x2="{x}" y2="{height - 20}" stroke="{C[kind][1]}" stroke-width="1.2" stroke-dasharray="4 4"/>')
    y = 140
    for i, st in enumerate(steps, 1):
        kind = st[0]
        if kind == 'msg':
            _, a, b, label, col, dash = st
            xa, xb = xs[a], xs[b]
            off = 5 if xb > xa else -5
            s.arrow([(xa + off, y), (xb - off, y)], col, dash=dash, sw=1.5)
            lx = (xa + xb) / 2
            s.badge(min(xa, xb) - 18, y, i, 'savc', r=9)
            s.lines(lx + 10, y - 16, label if isinstance(label, list) else [label], 11, color=INK)
            y += 60 if (isinstance(label, list) and len(label) > 1) else 52
        elif kind == 'self':
            _, a, label, col = st
            xa = xs[a]
            s.add(f'<path d="M{xa + 6},{y - 10} h34 v26 h-32" fill="none" stroke="{col}" stroke-width="1.4" marker-end="url(#ha)"/>')
            s.badge(xa - 18, y, i, 'savc', r=9)
            s.lines(xa + 48, y + 3, label if isinstance(label, list) else [label], 11, color=INK, anchor='start')
            y += 66 if (isinstance(label, list) and len(label) > 2) else 56
        elif kind == 'decide':
            _, a, label = st
            xa = xs[a]
            xa = (xs[a] + xs[a - 1]) / 2
            s.rect(xa - 130, y - 22, 260, 46, 'savc', r=8, fill='#ffffff')
            s.lines(xa, y + 1, label, 11, color=C['savc'][2], weight='bold')
            y += 56
    return xs, y


def fig_dualmode():
    H = 560
    s = Svg(1400, H)
    blue, green, orange = C['savc'][1], C['venue'][1], C['author'][1]
    lanesA = [('Venue rep.', 'venue'), ('SAVC', 'savc'), ('Author inbox', 'gray'), ('Author (portal)', 'author')]
    stepsA = [
        ('msg', 0, 1, ['post signed message', '(decision, fee request, ...)'], green, None),
        ('self', 1, ['store message,', 'anchor record in ledger'], blue),
        ('msg', 1, 2, ['notification only', '(no links, no content)'], LINE, '5 4'),
        ('msg', 3, 1, ['sign in via known address'], orange, None),
        ('msg', 1, 3, ['verified message + registered', 'payment account'], blue, None),
        ('decide', 3, ['email-only requests are', 'treated as fraudulent']),
    ]
    seq_panel(s, 20, '(a) Full Mediation mode', lanesA, stepsA, H)
    s.add(f'<line x1="700" y1="20" x2="700" y2="{H - 10}" stroke="#d8d7d2" stroke-width="1"/>')
    lanesB = [('Venue rep.', 'venue'), ('SAVC', 'savc'), ('Author', 'author')]
    stepsB = [
        ('msg', 0, 1, ['register outgoing message', '(recipient, type, digest)'], green, None),
        ('msg', 1, 0, ['VVC = reference + authenticator'], blue, None),
        ('msg', 0, 2, ['email carrying the VVC reference'], LINE, '5 4'),
        ('msg', 2, 1, ['enter reference on portal', '(known address)'], orange, None),
        ('self', 1, ['check record, recipient, digest,', 'expiry, key status, authenticator,', 'payment account'], blue),
        ('msg', 1, 2, ['valid / invalid + reason'], blue, None),
    ]
    seq_panel(s, 730, '(b) Verification Bridge mode', lanesB, stepsB, H)
    return s


# ============================================================ Fig 3: VVC structure and validation pipeline
def fig_vvc():
    s = Svg(1400, 600)
    s.text(30, 28, '(a) VVC structure', 14.5, 'bold', INK, anchor='start')
    # reference block
    s.rect(30, 50, 560, 210, 'savc', r=10)
    s.text(310, 76, 'Reference (human-readable, not secret)', 13, 'bold', C['savc'][2])
    parts = [('SAVC', 'issuing system'), ('2026', 'year'), ('ACC', 'message type'), ('45789', 'message ID')]
    x = 60
    for i, (p, d) in enumerate(parts):
        wbox = 105 if i < 3 else 125
        s.add(f'<rect x="{x}" y="104" width="{wbox}" height="48" rx="6" fill="white" stroke="{C["savc"][1]}" stroke-width="1.3"/>')
        s.text(x + wbox / 2, 128, p, 17, 'bold', INK)
        s.text(x + wbox / 2, 170, d, 11, color=INK2)
        if i < 3:
            s.text(x + wbox + 8, 128, '–', 18, 'bold', INK2)
        x += wbox + 16
    s.lines(310, 222, ['locates the record in the venue registry;', 'can be read over the phone or typed by the author'], 11.5)
    # plus
    s.text(615, 155, '+', 30, 'bold', INK2)
    # authenticator block
    s.rect(640, 50, 730, 210, 'ledger', r=10)
    s.text(1005, 76, 'Authenticator (secret-key protected, ≥ 128-bit security)', 13, 'bold', C['ledger'][2])
    s.add(f'<rect x="665" y="100" width="300" height="56" rx="6" fill="white" stroke="{C["ledger"][1]}" stroke-width="1.3"/>')
    s.lines(815, 128, ['HMAC-SHA256(K_rep, m)', 'or EdDSA signature over m'], 12, color=INK, weight='bold')
    s.arrow([(985, 128), (967, 128)], C['ledger'][1])
    fields = ['venue ID', 'representative ID', 'recipient address', 'message type', 'SHA-256(message)', 'expiry time']
    for i, fdx in enumerate(fields):
        cx = 995 + (i % 2) * 180
        cy = 104 + (i // 2) * 34
        s.add(f'<rect x="{cx}" y="{cy - 2}" width="168" height="28" rx="14" fill="white" stroke="#b9b2ee" stroke-width="1.1"/>')
        s.text(cx + 84, cy + 12, fdx, 11, color=INK)
    s.text(1170, 222, 'm = canonical encoding of the six bound fields', 11.5, color=INK2)
    s.text(815, 186, 'tag verified with the key recorded in the ledger', 11, color=INK2)
    # pipeline
    s.text(30, 312, '(b) Validation pipeline (each check rejects the attack classes shown)', 14.5, 'bold', INK, anchor='start')
    checks = [
        ('Record lookup', 'reference exists,', 'not revoked', 'A2'),
        ('Binding', 'recipient and type', 'match', 'A4'),
        ('Content digest', 'SHA-256 of received', 'message matches', 'A6'),
        ('Validity window', 'verification time', '≤ expiry', 'A5'),
        ('Key status', 'signing key active', 'in ledger', 'revoked keys'),
        ('Authenticator', 'constant-time', 'tag comparison', 'A3, A7'),
        ('Payment', 'fee account is a', 'registered account', 'A8'),
    ]
    bw, gap = 168, 22
    x = 30
    y = 345
    for i, (t, l1, l2, att) in enumerate(checks):
        s.rect(x, y, bw, 112, 'savc', r=8, fill='white')
        s.badge(x + 18, y + 18, i + 1, 'savc', r=10)
        s.text(x + bw / 2 + 10, y + 19, t, 12.5, 'bold', C['savc'][2])
        s.lines(x + bw / 2, y + 62, [l1, l2], 11)
        # reject arrow
        s.arrow([(x + bw / 2, y + 114), (x + bw / 2, y + 160)], C['attack'][1], sw=1.3)
        s.rect(x + 14, y + 162, bw - 28, 30, 'attack', r=15)
        s.text(x + bw / 2, y + 177, 'rejects ' + att, 11, 'bold', C['attack'][2])
        if i < len(checks) - 1:
            s.arrow([(x + bw + 2, y + 56), (x + bw + gap - 2, y + 56)], LINE)
        x += bw + gap
    s.text(30 + 3.5 * (bw + gap) - gap / 2, 575, 'A message is accepted only if all seven checks pass; failures return the reason to the author.', 12, color=INK2)
    return s


# ============================================================ Fig 4: registration and governance lifecycle
def fig_governance():
    s = Svg(1400, 440)
    stages = [
        ('Application', ['ISSN / organizing body', 'publisher or host institution', 'indexing records', 'domain-control proof', 'payment accounts'], 'venue'),
        ('Vetting', ['evidence checked against', 'authoritative sources', '(ISSN portal, indexes,', 'host institution, DNS)'], 'savc'),
        ('Activation', ['venue listed in registry', 'representatives receive', 'credentials and keys', 'accounts recorded'], 'savc'),
        ('Operation', ['Full Mediation or', 'Verification Bridge', 'communication with', 'authors'], 'savc'),
        ('Renewal / review', ['periodic re-verification', 'fraud reports from', 'authors and publishers'], 'savc'),
    ]
    bw, gap, y, h = 230, 42, 70, 190
    x = 30
    centers = []
    for i, (t, body, kind) in enumerate(stages):
        s.box(x, y, bw, h, t, body, kind, tsize=14, bsize=11.5)
        s.badge(x + 20, y + 20, i + 1, kind if kind != 'savc' else 'savc')
        centers.append(x + bw / 2)
        if i < len(stages) - 1:
            s.arrow([(x + bw + 2, y + h / 2), (x + bw + gap - 2, y + h / 2)], LINE)
        x += bw + gap
    # loop renewal -> operation
    s.arrow([(centers[4], y - 2), (centers[4], y - 32), (centers[3], y - 32), (centers[3], y - 2)], C['venue'][1],
            label='renewed', lpos=((centers[3] + centers[4]) / 2, y - 32), lsize=11)
    # suspension
    s.box(centers[4] - 140, 330, 280, 80, 'Suspension / revocation', ['venue or representative key', 'revoked; messages rejected'], 'attack')
    s.arrow([(centers[4], y + h + 2), (centers[4], 328)], C['attack'][1], label='credible fraud report', lpos=(centers[4] - 80, 295), lsize=11, lanchor='middle')
    # ledger band
    s.rect(30, 330, 960, 80, 'ledger', r=10)
    s.text(510, 356, 'Verification ledger', 13, 'bold', C['ledger'][2])
    s.text(510, 384, 'registration, key issuance, account registration, renewal, suspension and revocation events are appended', 11.5, color=INK2)
    for c in centers[1:4]:
        s.arrow([(c, y + h + 2), (c, 328)], C['ledger'][1], dash='4 3', sw=1.2)
    s.arrow([(centers[4] - 142, 370), (992, 370)], C['ledger'][1], dash='4 3', sw=1.2)
    s.text(30, 30, 'Scope: SAVC certifies the identity of a venue and its representatives, not its scientific quality.', 12, color=INK2, anchor='start', italic=True)
    return s


async def render(items):
    from playwright.async_api import async_playwright
    async with async_playwright() as p:
        b = await p.chromium.launch()
        for name, svg in items:
            open(f'{OUT}/{name}.svg', 'w').write(svg.svg())
            page = await b.new_page(viewport={'width': svg.w, 'height': svg.h}, device_scale_factor=3)
            await page.set_content(f'<html><body style="margin:0">{svg.svg()}</body></html>')
            await page.screenshot(path=f'{OUT}/{name}.png', clip={'x': 0, 'y': 0, 'width': svg.w, 'height': svg.h})
            await page.close()
        await b.close()


if __name__ == '__main__':
    items = [('fig_architecture', fig_architecture()), ('fig_dualmode', fig_dualmode()),
             ('fig_vvc', fig_vvc()), ('fig_governance', fig_governance())]
    asyncio.run(render(items))
    print('diagrams written')
