-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A file holds values it can write out and read back: not an access value,
-- a file, a protected object or anything holding an access value, and not a
-- multi-dimensional array. And a file is not a value to compare.

entity t_vhdl_filetype_bad is
end entity;

architecture a of t_vhdl_filetype_bad is
  type int_ptr is access integer;
  type rec_with_ptr is record
    p : int_ptr;
  end record;
  type matrix is array (1 to 2, 1 to 2) of integer;
  type int_file is file of integer;
  -- Legal
  type str_file is file of string;
  -- Illegal
  type bad1 is file of int_ptr;
  type bad2 is file of int_file;
  type bad3 is file of rec_with_ptr;
  type bad4 is file of matrix;
  file f1, f2 : int_file;
begin
  process
  begin
    assert f1 = f2;
    wait;
  end process;
end architecture;
