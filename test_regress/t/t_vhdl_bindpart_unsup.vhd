-- DESCRIPTION: Verilator: VHDL binding indication that associates part of
-- a port
--
-- "a(0) => i, a(1) => '1'" in a binding's port map connects the elements
-- of the entity's port one at a time (7.3.2.2). nvc 1.22.1, GHDL
-- 7.0.0-dev and Questa 2025.2 run it; here it is refused rather than
-- connected wrongly.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_bindpart_unsup_sub is
    port (a : in bit_vector(1 downto 0); y : out bit);
end entity;

architecture b of t_vhdl_bindpart_unsup_sub is
begin
    y <= a(0) and a(1);
end architecture;

entity t_vhdl_bindpart_unsup is
end entity;

architecture t of t_vhdl_bindpart_unsup is
    component c2 is
        port (i : in bit; r : out bit);
    end component;

    for all : c2
        use entity work.t_vhdl_bindpart_unsup_sub(b)
        port map (a(0) => i, a(1) => '1', y => r);

    signal s, t : bit;
begin
    g : component c2 port map (s, t);
end architecture;
