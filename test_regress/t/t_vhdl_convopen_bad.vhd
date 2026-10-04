-- DESCRIPTION: Verilator: VHDL conversion in the formal part of an open
-- port that gives an unconstrained subtype
--
-- An open port converted in the formal part takes its bounds from its
-- actual (t_vhdl_convopen), but what the conversion gives has to be
-- constrained: GHDL 7.0.0-dev and Questa 2025.2 refuse a type conversion to
-- STD_LOGIC_VECTOR and a function returning one, nvc 1.22.1 accepts both
-- (GHDL gna issue40, issue2422).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_convopen_bad_sub is
    port (foo : out std_ulogic_vector);
end entity;

architecture rtl of t_vhdl_convopen_bad_sub is
begin
    foo <= (foo'range => '0');
end architecture;

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_convopen_bad is
end entity;

architecture rtl of t_vhdl_convopen_bad is
    function convert (val : std_ulogic_vector) return std_logic_vector is
    begin
        return std_logic_vector(val);
    end function;

    signal a, b : std_logic_vector(7 downto 0);
begin
    u1 : entity work.t_vhdl_convopen_bad_sub port map (std_logic_vector(foo) => a);
    u2 : entity work.t_vhdl_convopen_bad_sub port map (convert(foo) => b);
end architecture;
