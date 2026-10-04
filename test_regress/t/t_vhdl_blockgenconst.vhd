-- DESCRIPTION: Verilator: VHDL block generic sizing the block's own
-- declarations
--
-- A block statement's generic is settled when the block is elaborated
-- (14.5.2), so a constant and signals the block declares can be sized by
-- it -- VESTS ashenden ch_16_fg_16_14's gated multiplexer, "constant zero :
-- bit_vector(0 to width - 1)" with width mapped to a constant, and a
-- generic left to its default the same.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_blockgenconst is
end entity;

architecture test of t_vhdl_blockgenconst is
  constant sig_width : positive := 16;
  signal s1, s2, s3 : bit_vector(0 to sig_width - 1);
  signal sel : bit;
begin
  mux : block is
    generic (width : positive; pad : natural := 2);
    generic map (width => sig_width);
    port (d0, d1 : in bit_vector(0 to width - 1);
          y : out bit_vector(0 to width - 1);
          sel : in bit);
    port map (d0 => s1, d1 => s2, y => s3, sel => sel);
    constant zero : bit_vector(0 to width - 1) := (others => '0');
    constant ones : bit_vector(0 to pad - 1) := (others => '1');
    signal gated_d0, gated_d1 : bit_vector(0 to width - 1);
  begin
    gated_d0 <= d0 when sel = '0' else zero;
    gated_d1 <= d1 when sel = '1' else zero;
    y <= (gated_d0 or gated_d1) xor (ones & zero(pad to width - 1));
  end block mux;

  process is
  begin
    s1 <= x"1111";
    s2 <= x"2222";
    sel <= '0';
    wait for 10 ns;
    assert s3 = x"D111" report "sel 0" severity failure;
    sel <= '1';
    wait for 10 ns;
    assert s3 = x"E222" report "sel 1" severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
