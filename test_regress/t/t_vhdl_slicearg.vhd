-- DESCRIPTION: Verilator: VHDL parameter associated slice by slice
--
-- "px(a(1 downto 0) => x, a(3 downto 2) => (others => '0'))" names the
-- parameter a part at a time (6.5.7.1); together the slices cover it. Each
-- was counted as an argument of its own, so no subprogram matched (GHDL gna
-- issue73). GHDL and Questa run this; nvc 1.22.1 crashes analysing it.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_slicearg is
end entity;
architecture test of t_vhdl_slicearg is
begin
  process is
    subtype xa_t is bit_vector(3 downto 0);
    procedure px(constant a : xa_t; k : integer) is
    begin
      assert a = "0011" and k = 5 severity failure;
    end procedure;
    procedure py(constant a : bit_vector(3 downto 0)) is
    begin
      assert a = "1001" severity failure;
    end procedure;
    procedure pw(constant b : in bit_vector(1 downto 0)) is
    begin
      px(a(1 downto 0) => b, a(3 downto 2) => (others => '0'), k => 5);
    end procedure;
    variable two : bit_vector(1 downto 0) := "01";
  begin
    px(a(1 downto 0) => "11", a(3 downto 2) => "00", k => 5);
    py(a(3 downto 2) => "10", a(1 downto 0) => two);
    pw("11");
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end;
