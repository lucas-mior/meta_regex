import csv
import matplotlib.pyplot as plt
import sys

prefix = 'benchmarks'

def parse_csv(filename):
    data = []
    with open(filename, mode='r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            row['count'] = int(row['count'])
            row['posix_time'] = float(row['posix_time'])
            row['meta_time'] = float(row['meta_time'])
            data.append(row)
    return data

def generate_plots(csv):
    try:
        data = parse_csv(csv)
    except FileNotFoundError:
        print(f"Error: {csv} not found. Execute the compiled C program first.")
        return

    # Filter benchmarks by suite type
    known_pairs = [r for r in data if r['suite'] == 'known_pairs']
    fuzzy_tests = [r for r in data if r['suite'] == 'fuzzy']
    file_fuzzy = [r for r in data if r['suite'] == 'file_fuzzy']

    # 1. Plot Fuzzy scaling benchmark (Line plot mapping size growth)
    if fuzzy_tests:
        fuzzy_tests.sort(key=lambda x: int(x['case']))
        sizes = [int(r['case']) for r in fuzzy_tests]
        posix_t = [r['posix_time'] for r in fuzzy_tests]
        meta_t = [r['meta_time'] for r in fuzzy_tests]

        plt.figure(figsize=(9, 5))
        plt.plot(sizes, posix_t, marker='o', color='#E69F00', linewidth=2, label='POSIX Engine')
        plt.plot(sizes, meta_t, marker='s', color='#009E73', linewidth=2, label='Meta Engine')
        plt.xscale('log', base=2)
        plt.xlabel('Maximum Input String Length (bytes)')
        plt.ylabel('Execution Time')
        plt.title('Performance Scaling: Fuzzy Random Test suite')
        plt.grid(True, which="both", linestyle="--", alpha=0.5)
        plt.legend()
        plt.tight_layout()
        plt.savefig(f'{prefix}/fuzzy_scaling_performance.png')
        plt.close()

    # 2. Plot Known Pairs benchmark metrics (Grouped Bar chart)
    if known_pairs:
        labels = [r['case'] for r in known_pairs]
        posix_t = [r['posix_time'] for r in known_pairs]
        meta_t = [r['meta_time'] for r in known_pairs]

        x = range(len(labels))
        width = 0.35

        plt.figure(figsize=(11, 6))
        plt.bar([i - width/2 for i in x], posix_t, width, label='POSIX Engine', color='#E69F00')
        plt.bar([i + width/2 for i in x], meta_t, width, label='Meta Engine', color='#009E73')
        plt.xticks(x, labels, rotation=30, ha='right')
        plt.ylabel('Execution Time')
        plt.title('Performance Comparison: Static Test Array Pairs')
        plt.legend()
        plt.grid(axis='y', linestyle='--', alpha=0.5)
        plt.tight_layout()
        plt.savefig(f'{prefix}/known_pairs_performance.png')
        plt.close()

    # 3. Plot File Fuzzy input corpus processing (Grouped Bar chart)
    if file_fuzzy:
        labels = [r['case'] for r in file_fuzzy]
        posix_t = [r['posix_time'] for r in file_fuzzy]
        meta_t = [r['meta_time'] for r in file_fuzzy]

        x = range(len(labels))
        width = 0.35

        plt.figure(figsize=(11, 6))
        plt.bar([i - width/2 for i in x], posix_t, width, label='POSIX Engine', color='#E69F00')
        plt.bar([i + width/2 for i in x], meta_t, width, label='Meta Engine', color='#009E73')
        plt.xticks(x, labels, rotation=30, ha='right')
        plt.ylabel('Execution Time')
        plt.title('Performance Comparison: Corpus Inputs directory tests')
        plt.legend()
        plt.grid(axis='y', linestyle='--', alpha=0.5)
        plt.tight_layout()
        plt.savefig(f'{prefix}/file_fuzzy_performance.png')
        plt.close()

    print("Success: Generated metric plots tracking performance comparisons.")

if __name__ == '__main__':
    generate_plots(sys.argv[1])
