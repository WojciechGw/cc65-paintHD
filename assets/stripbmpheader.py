import os
import argparse
from typing import NoReturn # Used for type hinting purposes

def truncate_binary_file_stream(
    input_file_path: str, 
    output_file_path: str, 
    bytes_to_skip: int = 63
) -> None:
    """
    Truncates the beginning of a binary file by skipping a specified number 
    of bytes and writes the rest of the content to a new file.
    
    This streaming method is highly efficient for very large files.

    Args:
        input_file_path: The path to the binary file to be processed.
        output_file_path: The path where the truncated data will be saved.
        bytes_to_skip: The number of initial bytes (header) to discard.
    """
    print("="*60)
    print(f"STARTING TRUNCATION PROCESS:")
    print(f"  Input File: {input_file_path}")
    print(f"  Output File: {output_file_path}")
    print(f"  Bytes to Skip: {bytes_to_skip}")
    print("="*60)

    try:
        # Use 'rb' for reading (read binary) and 'wb' for writing (write binary)
        with open(input_file_path, 'rb') as input_file, open(output_file_path, 'wb') as output_file:
            
            # 1. Skip the specified header bytes
            # This reads the bytes but discards them from the final output.
            header_bytes = input_file.read(bytes_to_skip)
            
            # 2. Copy the remaining data stream
            # Read all remaining bytes from the current pointer position 
            # and write them directly to the output file.
            remaining_data = input_file.read()
            output_file.write(remaining_data)

        print("\n=============================================================")
        print(f"SUCCESS: Truncation completed successfully!")
        print("================================")

    except FileNotFoundError:
        print(f"\nERROR: Input file not found at path: {input_file_path}")
        # Exit with a failure code
        exit(1) 
    except Exception as e:
        print(f"\nAN ERROR OCCURRED: {e}")
        # Exit with a failure code
        exit(1)


# --- Argument Parsing and Execution Block ---
if __name__ == "__main__":
    import argparse
    
    # Initialize the argument parser
    parser = argparse.ArgumentParser(
        description="Truncates the beginning of a binary file by skipping a set number of header bytes."
    )
    
    # Define the required argument for the input file
    parser.add_argument(
        "input_file", 
        type=str, 
        help="The path to the binary file that needs to be truncated."
    )
    
    # Define the optional argument for the output file
    parser.add_argument(
        "output_file", 
        type=str, 
        help="The path where the resulting truncated file will be saved."
    )

    # Define the optional argument for the number of bytes to skip
    parser.add_argument(
        "--skip", 
        type=int, 
        default=62, 
        help="Number of initial bytes (header) to discard. (Default: 63)"
    )
    
    # Parse the arguments provided when running the script
    args = parser.parse_args()
    
    # Now call the core function using the arguments provided by the user
    truncate_binary_file_stream(
        input_file_path=args.input_file, 
        output_file_path=args.output_file, 
        bytes_to_skip=args.skip
    )
