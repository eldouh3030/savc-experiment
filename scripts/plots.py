import csv, sys, collections, statistics as st
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib import font_manager
import numpy as np

RES = sys.argv[1] if len(sys.argv) > 1 else 'results'
OUT = sys.argv[2] if len(sys.argv) > 2 else 'figures'
import os; os.makedirs(OUT, exist_ok=True)

fam = 'Liberation Sans' if any('Liberation Sans' in f.name for f in font_manager.fontManager.ttflist) else 'DejaVu Sans'
plt.rcParams.update({
    'font.family': fam, 'font.size': 8.5, 'axes.titlesize': 9, 'axes.labelsize': 8.5,
    'xtick.labelsize': 8, 'ytick.labelsize': 8, 'legend.fontsize': 7.5,
    'axes.edgecolor': '#52514e', 'axes.linewidth': 0.6, 'xtick.color': '#52514e', 'ytick.color': '#52514e',
    'axes.labelcolor': '#0b0b0b', 'text.color': '#0b0b0b', 'axes.spines.top': False, 'axes.spines.right': False,
    'axes.grid': True, 'grid.color': '#e4e3df', 'grid.linewidth': 0.5, 'axes.axisbelow': True,
    'savefig.dpi': 600, 'savefig.bbox': 'tight', 'savefig.pad_inches': 0.03, 'pdf.fonttype': 42,
})
INK2 = '#52514e'
DEFS = ['DMARC only', 'Whitelist only', 'Whitelist + DMARC', 'SAVC (VVC only)', 'SAVC (VVC + payment)']
COL = dict(zip(DEFS, ['#2a78d6', '#eb6834', '#1baf7a', '#eda100', '#e87ba4']))
# secondary encoding for print / CVD
MRK = dict(zip(DEFS, ['o', 's', '^', 'D', 'v']))
LS = dict(zip(DEFS, ['-', '--', '-.', ':', '-']))
SAVC_DARK = '#184f95'


def save(fig, name):
    fig.savefig(f'{OUT}/{name}.png')
    fig.savefig(f'{OUT}/{name}.pdf')
    plt.close(fig)

# ---------------------------------------------------------------- Fig: detection heatmap
rows = list(csv.DictReader(open(f'{RES}/detection_by_class.csv')))
classes = []
for r in rows:
    if r['class'] not in classes: classes.append(r['class'])
M = np.zeros((len(classes), len(DEFS)))
for r in rows:
    M[classes.index(r['class']), DEFS.index(r['defense'])] = float(r['mean_rate']) * 100

attack_idx = [i for i, c in enumerate(classes) if c.startswith('A')]
legit_idx = [i for i, c in enumerate(classes) if not c.startswith('A')]

from matplotlib.colors import LinearSegmentedColormap
seq = LinearSegmentedColormap.from_list('seqblue', ['#f4f8fd', '#cde2fb', '#86b6ef', '#3987e5', '#1c5cab', '#0d366b'])

fig, axes = plt.subplots(2, 1, figsize=(7.0, 4.9), gridspec_kw={'height_ratios': [len(attack_idx), len(legit_idx)], 'hspace': 0.42})
for ax, idx, title in [(axes[0], attack_idx, '(a) Detection rate of fraudulent messages (%)'),
                       (axes[1], legit_idx, '(b) Acceptance rate of legitimate messages (%)')]:
    sub = M[idx]
    ax.imshow(sub, cmap=seq, vmin=0, vmax=100, aspect='auto')
    ax.set_yticks(range(len(idx)))
    ax.set_yticklabels([classes[i] for i in idx])
    ax.set_xticks(range(len(DEFS)))
    ax.set_xticklabels(DEFS if ax is axes[1] else [])
    ax.tick_params(length=0)
    ax.grid(False)
    for s in ax.spines.values(): s.set_visible(False)
    ax.set_xticks(np.arange(-.5, len(DEFS), 1), minor=True)
    ax.set_yticks(np.arange(-.5, len(idx), 1), minor=True)
    ax.grid(which='minor', color='white', linewidth=2)
    ax.tick_params(which='minor', length=0)
    for i in range(sub.shape[0]):
        for j in range(sub.shape[1]):
            v = sub[i, j]
            ax.text(j, i, f'{v:.1f}', ha='center', va='center', fontsize=7.5,
                    color='white' if v > 55 else '#0b0b0b', fontweight='bold' if j >= 3 else 'normal')
    ax.set_title(title, loc='left', fontweight='bold')
axes[1].tick_params(axis='x', labelsize=7.8)
save(fig, 'fig_detection_heatmap')

# ---------------------------------------------------------------- Fig: sensitivity
rows = list(csv.DictReader(open(f'{RES}/sensitivity.csv')))
GROUPS = [('DMARC only', 'DMARC only  /  Whitelist + DMARC', '#2a78d6', 'o', '-'),
          ('Whitelist only', 'Whitelist only', '#eb6834', 's', '--'),
          ('SAVC (VVC + payment)', 'SAVC (both variants)', '#1c5cab', 'D', '-')]
GROUPS[2] = ('SAVC (VVC + payment)', 'SAVC (both variants)', '#1baf7a', 'D', '-')
fig, axes = plt.subplots(1, 2, figsize=(7.0, 2.7), sharey=True)
panels = [('forwarding_rate', '(a) Legitimate mail relayed through forwarders', 'Share of legitimate messages relayed (%)'),
          ('unregistered_rate', '(b) Venue omits registering a message', 'Share of unregistered legitimate messages (%)')]
for ax, (param, title, xl) in zip(axes, panels):
    for d, lab, c, m, ls in GROUPS:
        pts = [(float(r['value']) * 100, float(r['FPR']) * 100) for r in rows if r['parameter'] == param and r['defense'] == d]
        x, y = zip(*pts)
        ax.plot(x, y, color=c, ls=ls, marker=m, ms=4.2, lw=1.5, label=lab, markeredgecolor='white', markeredgewidth=0.5)
    ax.set_title(title, loc='left', fontweight='bold')
    ax.set_xlabel(xl)
    ax.set_xlim(-0.3, 10.3)
    ax.set_ylim(-0.4, 10.8)
axes[0].set_ylabel('False rejection rate (%)')
h, l = axes[0].get_legend_handles_labels()
fig.legend(h, l, loc='upper center', ncol=3, frameon=False, bbox_to_anchor=(0.5, 1.06))
fig.tight_layout(rect=(0, 0, 1, 0.94), w_pad=2)
save(fig, 'fig_sensitivity')

# ---------------------------------------------------------------- Fig: scalability
lat = list(csv.DictReader(open(f'{RES}/latency_scaling.csv')))
led = list(csv.DictReader(open(f'{RES}/ledger.csv')))
fig, axes = plt.subplots(1, 2, figsize=(7.0, 2.6))
ax = axes[0]
ops = [('issue_with_ledger', 'VVC issuance (incl. ledger append)', '#2a78d6', 'o', '-'),
       ('verify_registry_only', 'Verification (registry + HMAC + payment)', '#eb6834', 's', '--'),
       ('verify_with_ledger_check', 'Verification + ledger key check', '#1baf7a', '^', '-.')]
for op, lab, c, m, ls in ops:
    pts = [(int(r['registry_size']), float(r['median_us']), float(r['min_us']), float(r['max_us'])) for r in lat if r['operation'] == op]
    x, y, lo, hi = map(np.array, zip(*pts))
    ax.errorbar(x, y, yerr=[y - lo, hi - y], color=c, marker=m, ls=ls, lw=1.4, ms=4.2, label=lab, capsize=2.5,
                elinewidth=0.8, markeredgecolor='white', markeredgewidth=0.5)
ax.set_xscale('log')
ax.set_xlabel('Number of registered VVC records')
ax.set_ylabel('Latency per operation (µs)')
ax.set_ylim(0, 10)
ax.set_title('(a) Per-operation latency', loc='left', fontweight='bold')
ax.legend(frameon=False, loc='upper left', fontsize=6.8, ncol=1)
ax = axes[1]
x = [int(r['chain_length']) for r in led]
y = [float(r['full_validation_ms']) for r in led]
ax.plot(x, y, color='#2a78d6', marker='o', lw=1.4, ms=4.2, markeredgecolor='white', markeredgewidth=0.5)
for k, (xi, yi) in enumerate(zip(x, y)):
    ax.annotate((f'{yi:.2f} ms' if yi < 1 else f'{yi:.1f} ms') if yi < 100 else f'{yi:.0f} ms', (xi, yi), textcoords='offset points',
                xytext=(7, -3) if k == 0 else (-6, 5), ha='left' if k == 0 else 'right', fontsize=7, color=INK2)
ax.set_xscale('log'); ax.set_yscale('log')
ax.set_xlabel('Ledger length (blocks)')
ax.set_ylabel('Full chain validation time (ms)')
ax.set_title('(b) Ledger integrity validation', loc='left', fontweight='bold')
ax.set_ylim(min(y) / 2, max(y) * 4)
fig.tight_layout(w_pad=2.5)
save(fig, 'fig_scalability')
print('charts written to', OUT)
