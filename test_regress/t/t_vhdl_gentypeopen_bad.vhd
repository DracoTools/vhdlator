-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A generic type given an unconstrained array type leaves the variables the
-- instance declares of it without bounds (6.4.2.4). References: nvc 1.22.1,
-- GHDL 7.0.0-dev (Questa 2025.2 takes it).

package gen is
  generic (type user_type);
  type storage_t is protected
    procedure add_entry (entry : user_type);
  end protected;
end package;

package body gen is
  type storage_t is protected body
    variable user : user_type;
    procedure add_entry (entry : user_type) is
    begin
      user := entry;
    end procedure;
  end protected body;
end package body;

entity t is
end entity;

architecture a of t is
  package ok_pkg is new work.gen generic map (bit_vector(7 downto 0));
  package bad_pkg is new work.gen generic map (bit_vector);  -- Bad
begin
end architecture;
