-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- 'ELEMENT of an access to an array whose element subtype is open: the
-- allocator gave the elements their bounds, which 'ELEMENT'LENGTH and the
-- other bounds of it report (16.2.3; GHDL gna issue2166:repro4). GHDL and
-- Questa agree; nvc refuses an access prefix.

entity t_vhdl_accelemlen is
end entity;

architecture behav of t_vhdl_accelemlen is
  subtype membase_t is integer_vector;
  type memblock_t is array (integer range <>) of membase_t;
  type memblock_ptr is access memblock_t;
begin
  process
    variable mem : memblock_ptr;
    variable width : natural := 4;
  begin
    mem := new memblock_t(0 to width - 1)(3 to 34);
    assert mem'length = 4 report "mem'length /= 4" severity failure;
    assert mem'element'length = 32 report "mem'element'length /= 32" severity failure;
    assert mem'element'left = 3 report "mem'element'left /= 3" severity failure;
    assert mem'element'high = 34 report "mem'element'high /= 34" severity failure;
    assert mem.all'element'length = 32 report "mem.all'element'length /= 32"
      severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
