import csv
import matplotlib.pyplot as plt
import sys
import os

prefix = 'benchmarks'

def parse_csv(filename):
    data = []
    with open(filename, mode='r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            row['count'] = int(row['count'])
            row['libc_time'] = float(row['libc_time'])
            row['meta_time'] = float(row['meta_time'])
            data.append(row)
    return data

def generate_plots(csv_filename):
    # Extract the file name (without path or extension) to use as a prefix
    base_name = os.path.splitext(os.path.basename(csv_filename))[0]

    try:
        data = parse_csv(csv_filename)
    except FileNotFoundError:
        print(f"Error: {csv_filename} not found. Execute the compiled C program first.")
        return

    # Color configurations
    c_libc_ex = '#E69F00'  # Orange
    c_libc_nx = '#F5C767'  # Light Orange
    c_meta_ex  = '#009E73'  # Green
    c_meta_nx  = '#85D3B1'  # Light Green

    # 1. Plot Fuzzy scaling benchmark (Line plot mapping size growth)
    fuzzy_cases = sorted(list(set(int(r['case']) for r in data if r['suite'] in ['fuzzy_extract', 'fuzzy_no_extract'])))
    if fuzzy_cases:
        f_ex = {int(r['case']): r for r in data if r['suite'] == 'fuzzy_extract'}
        f_nx = {int(r['case']): r for r in data if r['suite'] == 'fuzzy_no_extract'}

        plt.figure(figsize=(10, 6))

        if f_ex:
            p_ex_t = [f_ex[s]['libc_time'] for s in fuzzy_cases if s in f_ex]
            m_ex_t = [f_ex[s]['meta_time'] for s in fuzzy_cases if s in f_ex]
            plt.plot(fuzzy_cases, p_ex_t, marker='o', color=c_libc_ex, linewidth=2, label='libc (Extracting)')
            plt.plot(fuzzy_cases, m_ex_t, marker='s', color=c_meta_ex, linewidth=2, label='Meta (Extracting)')
            
        if f_nx:
            p_nx_t = [f_nx[s]['libc_time'] for s in fuzzy_cases if s in f_nx]
            m_nx_t = [f_nx[s]['meta_time'] for s in fuzzy_cases if s in f_nx]
            plt.plot(fuzzy_cases, p_nx_t, marker='o', linestyle='--', color=c_libc_nx, linewidth=2, label='libc (Non-Extracting)')
            plt.plot(fuzzy_cases, m_nx_t, marker='s', linestyle='--', color=c_meta_nx, linewidth=2, label='Meta (Non-Extracting)')

        plt.xscale('log', base=2)
        plt.xlabel('Maximum Input String Length (bytes)')
        plt.ylabel('Execution Time')
        plt.title('Performance Scaling: Fuzzy Random Test suite')
        plt.grid(True, which="both", linestyle="--", alpha=0.5)
        plt.legend()
        plt.tight_layout()
        plt.savefig(f'{prefix}/{base_name}_fuzzy_scaling_performance.png')
        plt.close()

    # 2. Plot Known Pairs benchmark metrics (Grouped Bar chart)
    known_cases = []
    for r in data:
        if r['suite'] in ['known_pairs_extract', 'known_pairs_no_extract'] and r['case'] not in known_cases:
            known_cases.append(r['case'])

    if known_cases:
        kp_ex = {r['case']: r for r in data if r['suite'] == 'known_pairs_extract'}
        kp_nx = {r['case']: r for r in data if r['suite'] == 'known_pairs_no_extract'}

        libc_ex = [kp_ex[c]['libc_time'] if c in kp_ex else 0.0 for c in known_cases]
        meta_ex  = [kp_ex[c]['meta_time'] if c in kp_ex else 0.0 for c in known_cases]
        libc_nx = [kp_nx[c]['libc_time'] if c in kp_nx else 0.0 for c in known_cases]
        meta_nx  = [kp_nx[c]['meta_time'] if c in kp_nx else 0.0 for c in known_cases]

        x = range(len(known_cases))
        width = 0.20

        plt.figure(figsize=(12, 6))
        plt.bar([i - 1.5*width for i in x], libc_ex, width, label='libc (Extracting)', color=c_libc_ex)
        plt.bar([i - 0.5*width for i in x], meta_ex,  width, label='Meta (Extracting)', color=c_meta_ex)
        plt.bar([i + 0.5*width for i in x], libc_nx, width, label='libc (Non-Extracting)', color=c_libc_nx)
        plt.bar([i + 1.5*width for i in x], meta_nx,  width, label='Meta (Non-Extracting)', color=c_meta_nx)
        
        plt.xticks(x, known_cases, rotation=30, ha='right')
        plt.ylabel('Execution Time')
        plt.title('Performance Comparison: Static Test Array Pairs')
        plt.legend()
        plt.grid(axis='y', linestyle='--', alpha=0.5)
        plt.tight_layout()
        plt.savefig(f'{prefix}/{base_name}_known_pairs_performance.png')
        plt.close()

    # 3. Plot File Fuzzy input corpus processing (Grouped Bar chart)
    file_cases = []
    for r in data:
        if r['suite'] in ['file_fuzzy_extract', 'file_fuzzy_no_extract'] and r['case'] not in file_cases:
            file_cases.append(r['case'])

    if file_cases:
        ff_ex = {r['case']: r for r in data if r['suite'] == 'file_fuzzy_extract'}
        ff_nx = {r['case']: r for r in data if r['suite'] == 'file_fuzzy_no_extract'}

        libc_ex = [ff_ex[c]['libc_time'] if c in ff_ex else 0.0 for c in file_cases]
        meta_ex  = [ff_ex[c]['meta_time'] if c in ff_ex else 0.0 for c in file_cases]
        libc_nx = [ff_nx[c]['libc_time'] if c in ff_nx else 0.0 for c in file_cases]
        meta_nx  = [ff_nx[c]['meta_time'] if c in ff_nx else 0.0 for c in file_cases]

        x = range(len(file_cases))
        width = 0.20

        plt.figure(figsize=(12, 6))
        plt.bar([i - 1.5*width for i in x], libc_ex, width, label='libc (Extracting)', color=c_libc_ex)
        plt.bar([i - 0.5*width for i in x], meta_ex,  width, label='Meta (Extracting)', color=c_meta_ex)
        plt.bar([i + 0.5*width for i in x], libc_nx, width, label='libc (Non-Extracting)', color=c_libc_nx)
        plt.bar([i + 1.5*width for i in x], meta_nx,  width, label='Meta (Non-Extracting)', color=c_meta_nx)
        
        plt.xticks(x, file_cases, rotation=30, ha='right')
        plt.ylabel('Execution Time')
        plt.title('Performance Comparison: Corpus Inputs directory tests')
        plt.legend()
        plt.grid(axis='y', linestyle='--', alpha=0.5)
        plt.tight_layout()
        plt.savefig(f'{prefix}/{base_name}_file_fuzzy_performance.png')
        plt.close()

    print("Success: Generated metric plots tracking performance comparisons.")

if __name__ == '__main__':
    generate_plots(sys.argv[1])
