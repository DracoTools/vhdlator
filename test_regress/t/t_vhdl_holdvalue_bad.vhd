-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A constant, a signal, a generic and a port hold values, and a value is
-- never an access, a file or a protected object. A constant given no value
-- is deferred, which only a package declaration may do. An interface's
-- default value is of its type.

package t_vhdl_holdvalue_pkg is
  type int_ptr is access integer;
  type int_file is file of integer;
  constant deferred_ok : integer;
end package;

package body t_vhdl_holdvalue_pkg is
  constant deferred_ok : integer := 3;
end package body;

use work.t_vhdl_holdvalue_pkg.all;

entity t_vhdl_holdvalue_bad is
  generic (
    g_ok : integer := 1;
    g_bad : integer := true
  );
  port (
    p_ok : in bit := '0';
    p_bad : in bit := 5
  );
end entity;

architecture a of t_vhdl_holdvalue_bad is
  constant c_ptr : int_ptr := null;
  signal s_ptr : int_ptr;
  constant c_deferred : integer;
begin
end architecture;
