#!/usr/bin/env python3
"""
Barebox Configuration Tracker - Using Existing Tools

This script leverages existing tools (git, build system) to find CONFIG option
file associations instead of reinventing file parsing.

Usage:
    python3 sma_config_tracker.py --config ./build/.config --source . --output omap_config.csv
"""

import argparse
import os
import subprocess
import csv
import sys
from pathlib import Path
from typing import Dict, List, Set, Tuple
import re


class BareboxConfigTracker:
    def __init__(self, config_path: str, source_path: str):
        self.config_path = Path(config_path)
        self.source_path = Path(source_path)
        self.config_options = {}
        
    def parse_config_file(self) -> Dict[str, str]:
        """Parse the configuration file and extract CONFIG options."""
        print(f"Parsing config file: {self.config_path}")
        
        if not self.config_path.exists():
            raise FileNotFoundError(f"Config file not found: {self.config_path}")
        
        config_options = {}
        
        with open(self.config_path, 'r', encoding='utf-8', errors='ignore') as f:
            for line_num, line in enumerate(f, 1):
                line = line.strip()
                
                # Skip comments and empty lines
                if not line or line.startswith('#'):
                    continue
                
                # Match CONFIG_OPTION=value or CONFIG_OPTION (for =y)
                match = re.match(r'^(CONFIG_[A-Z_0-9]+)(?:=(.+))?$', line)
                if match:
                    option, value = match.groups()
                    config_options[option] = value if value else 'y'
                    
        print(f"Found {len(config_options)} configuration options")
        return config_options
    
    def find_makefile_objects(self, config_option: str) -> Set[str]:
        """Find .o files exactly as referenced in Makefiles for this CONFIG option."""
        files = set()
        
        try:
            os.chdir(self.source_path)
            
            # Only search for Makefile obj-$(CONFIG_OPTION) patterns
            pattern = f'obj-.*{config_option}'
            
            result = subprocess.run(['git', 'grep', '-n', pattern],
                                  capture_output=True, text=True, timeout=15)
            
            if result.returncode == 0:
                for line in result.stdout.split('\n'):
                    if line and ':' in line:
                        # Parse: filename:line_number:content
                        parts = line.split(':', 2)
                        if len(parts) >= 3:
                            makefile_path = parts[0]
                            makefile_line = parts[2]
                            
                            # Only process if it's a Makefile
                            if 'Makefile' in makefile_path or 'makefile' in makefile_path:
                                # Extract .o files exactly as they appear in Makefile
                                obj_files = re.findall(r'([a-zA-Z0-9_/-]+\.o)', makefile_line)
                                
                                for obj_file in obj_files:
                                    # Add the .o file exactly as it appears in the Makefile
                                    makefile_dir = Path(makefile_path).parent
                                    if makefile_dir == Path('.'):
                                        # If in root directory, just use the filename
                                        files.add(obj_file)
                                    else:
                                        # Preserve the path structure from Makefile
                                        files.add(str(makefile_dir / obj_file))
                    
        except (subprocess.CalledProcessError, FileNotFoundError, OSError, subprocess.TimeoutExpired):
            print(f"  Warning: Makefile search failed for {config_option}")
        
        return files
    
    def makefile_analysis(self, config_option: str) -> Set[str]:
        """Analyze Makefile obj-y patterns for CONFIG option."""
        files = set()
        
        try:
            os.chdir(self.source_path)
            
            # Find all Makefile references
            result = subprocess.run(['find', '.', '-name', 'Makefile', '-exec', 'grep', '-H', config_option, '{}', '+'],
                                  capture_output=True, text=True, timeout=30)
            
            if result.returncode == 0:
                for line in result.stdout.split('\n'):
                    if line and ':' in line:
                        makefile_path = line.split(':')[0].lstrip('./')
                        files.add(makefile_path)
                        
                        # Parse the makefile line to find referenced .c files
                        makefile_line = line.split(':', 1)[1]
                        # Look for .o references which correspond to .c files
                        obj_matches = re.findall(r'([a-zA-Z0-9_-]+)\.o', makefile_line)
                        for obj_name in obj_matches:
                            # Try to find corresponding .c file
                            c_file = Path(makefile_path).parent / f"{obj_name}.c"
                            if c_file.exists():
                                files.add(str(c_file))
                                
        except (subprocess.CalledProcessError, subprocess.TimeoutExpired):
            print(f"  Warning: Makefile analysis failed for {config_option}")
        
        return files
    
    def kconfig_analysis(self, config_option: str) -> Set[str]:
        """Analyze Kconfig files for dependencies and definitions."""
        files = set()
        base_option = config_option.replace('CONFIG_', '')
        
        try:
            os.chdir(self.source_path)
            
            # Find Kconfig files that reference this option
            patterns = [
                f'config {base_option}',
                f'select {base_option}',
                f'depends on {config_option}',
                config_option
            ]
            
            for pattern in patterns:
                result = subprocess.run(['find', '.', '-name', 'Kconfig*', '-exec', 'grep', '-l', pattern, '{}', '+'],
                                      capture_output=True, text=True, timeout=15)
                if result.returncode == 0:
                    for file in result.stdout.strip().split('\n'):
                        if file and not file.startswith('.git'):
                            files.add(file.lstrip('./'))
                            
        except (subprocess.CalledProcessError, subprocess.TimeoutExpired):
            print(f"  Warning: Kconfig analysis failed for {config_option}")
        
        return files
    
    def build_system_analysis(self) -> Dict[str, Set[str]]:
        """Use build system to find file associations (if possible)."""
        print("Attempting build system analysis...")
        associations = {}
        
        try:
            os.chdir(self.source_path)
            
            # Try to run make with dry-run to see what would be built
            result = subprocess.run(['make', '-n'], capture_output=True, text=True, timeout=30)
            
            if result.returncode == 0:
                print("  Build system analysis successful")
                # Parse build commands to extract .c/.o files
                for line in result.stdout.split('\n'):
                    if '.c' in line or '.o' in line:
                        # Extract filenames from gcc commands
                        c_files = re.findall(r'([a-zA-Z0-9_/-]+\.c)', line)
                        for c_file in c_files:
                            if c_file not in associations:
                                associations[c_file] = set()
            else:
                print("  Build system analysis failed - using alternative methods")
                
        except subprocess.TimeoutExpired:
            print("  Build system analysis timed out")
        except Exception as e:
            print(f"  Build system analysis error: {e}")
            
        return associations
    
    def find_associated_files(self, config_option: str) -> Set[str]:
        """Find .o files associated with a CONFIG option from Makefiles only."""
        print(f"  Searching Makefiles for: {config_option}")
        
        # Only search Makefiles for obj-$(CONFIG_OPTION) patterns
        makefile_objects = self.find_makefile_objects(config_option)
        
        print(f"    Found: {len(makefile_objects)} .o files")
        
        return makefile_objects
    
    def analyze_configuration(self) -> Dict[str, Dict]:
        """Analyze the configuration and find .c/.o files from Makefiles only."""
        print("Starting Makefile object file analysis...")
        
        # Parse config file
        self.config_options = self.parse_config_file()
        
        # Find associated files for each option
        results = {}
        total_options = len(self.config_options)
        
        for i, (option, value) in enumerate(self.config_options.items(), 1):
            print(f"[{i}/{total_options}] Processing {option}")
            
            associated_files = self.find_associated_files(option)
            
            results[option] = {
                'value': value,
                'files': sorted(list(associated_files)),
                'tracked': False  # Default to not tracked
            }
            
            obj_count = len(associated_files)
            print(f"    Found: {obj_count} .o files")
        
        return results
    
    def export_to_csv(self, results: Dict[str, Dict], output_path: str):
        """Export results to CSV format for Google Sheets."""
        print(f"Exporting results to: {output_path}")
        
        with open(output_path, 'w', newline='', encoding='utf-8') as csvfile:
            writer = csv.writer(csvfile)
            
            # Write header
            writer.writerow(['Track Status', 'CONFIG_OPTION', 'Object Files'])
            
            # Write data
            for option, data in sorted(results.items()):
                track_status = 'NO'  # Default to NO for tracking
                
                # Files are already .o files from Makefile, just join them
                files_str = '; '.join(data['files']) if data['files'] else ''
                
                writer.writerow([track_status, option, files_str])
        
        print(f"Exported {len(results)} configuration options to CSV")
    
    def print_summary(self, results: Dict[str, Dict]):
        """Print a summary of the analysis."""
        total_options = len(results)
        with_objects = sum(1 for data in results.values() if data['files'])
        without_objects = total_options - with_objects
        
        print("\n" + "="*60)
        print("MAKEFILE .o FILES ANALYSIS")
        print("="*60)
        print(f"Total CONFIG options: {total_options}")
        print(f"Options with .o files in Makefiles: {with_objects}")
        print(f"Options without .o files: {without_objects}")
        
        # Show top options with most .o files
        print(f"\nTop 10 options with most .o files:")
        top_options = sorted(results.items(), 
                           key=lambda x: len(x[1]['files']), 
                           reverse=True)[:10]
        
        for option, data in top_options:
            if data['files']:
                print(f"  {option}: {len(data['files'])} .o files")
        
        print("\n" + "="*60)


def main():
    parser = argparse.ArgumentParser(
        description='Extract barebox CONFIG options using existing tools (git, build system)',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
This script finds .c/.o files referenced in Makefiles for CONFIG options.
It searches for obj-$(CONFIG_OPTION) patterns and extracts the object files.

Examples:
  python barebox_tracker.py --config .config --source . --output tracker.csv
  python barebox_tracker.py --config configs/omap_defconfig --source . --output omap.csv

Output CSV format:
  Track Status, CONFIG_OPTION, Object Files
  NO, CONFIG_SERIAL, serial.o
  NO, CONFIG_NET, net.o; dhcp.o; tftp.o
        '''
    )
    
    parser.add_argument('--config', '-c', required=True,
                       help='Path to barebox config file (.config or *defconfig)')
    parser.add_argument('--source', '-s', required=True,
                       help='Path to barebox source directory')
    parser.add_argument('--output', '-o', default='barebox_config_tracker.csv',
                       help='Output CSV file path (default: barebox_config_tracker.csv)')
    
    args = parser.parse_args()
    
    try:
        # Validate inputs
        config_path = Path(args.config)
        source_path = Path(args.source)
        
        if not config_path.exists():
            print(f"Error: Config file not found: {config_path}")
            sys.exit(1)
            
        if not source_path.exists() or not source_path.is_dir():
            print(f"Error: Source directory not found: {source_path}")
            sys.exit(1)
        
        # Check if we're in a git repository
        try:
            subprocess.run(['git', 'rev-parse', '--git-dir'], 
                         cwd=source_path, capture_output=True, check=True)
        except subprocess.CalledProcessError:
            print("Warning: Not a git repository. git grep functionality will be limited.")
        
        # Create tracker and analyze
        tracker = BareboxConfigTracker(args.config, args.source)
        results = tracker.analyze_configuration()
        
        # Export to CSV
        tracker.export_to_csv(results, args.output)
        
        # Print summary
        tracker.print_summary(results)
        
        print(f"\n✅ Success! Import {args.output} into Google Sheets:")
        print("   1. Open Google Sheets")
        print("   2. File -> Import -> Upload")
        print(f"   3. Select {args.output}")
        print("   4. Choose 'Replace spreadsheet' and 'Detect automatically'")
        
    except KeyboardInterrupt:
        print("\n❌ Analysis interrupted by user")
        sys.exit(1)
    except Exception as e:
        print(f"❌ Error: {e}")
        sys.exit(1)


if __name__ == '__main__':
    main()
