-- DESCRIPTION: Verilator: VHDL call through a subprogram alias, per actual
--
-- An alias of a subprogram denotes the subprogram (6.6.3), so each call
-- written on it sees the bounds of its own actual.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_aliascopy is
end entity;

architecture test of t_vhdl_aliascopy is

    -- As long as the argument says, and saying where the argument starts
    function enc (constant data : bit_vector) return string is
        variable s : string(1 to 2 + data'length) := (others => '.');
    begin
        s(1) := character'val(48 + data'left);
        s(2) := character'val(48 + data'right);
        return s;
    end function;

    procedure put (constant data : in bit_vector; variable l, r : out integer) is
    begin
        l := data'left;
        r := data'right;
    end procedure;

    alias enc_bv is enc[bit_vector return string];
    alias put_bv is put[bit_vector, integer, integer];

begin

    process is
        variable bv : bit_vector(5 downto 3);
        variable up : bit_vector(4 to 6);
        variable l, r : integer;
    begin
        -- Three actuals of one length, each with bounds of its own
        assert enc_bv("101") = "02..." severity failure;
        assert enc_bv(bv) = "53..." severity failure;
        assert enc_bv(up) = "46..." severity failure;
        assert enc(bv) = "53..." severity failure;
        put_bv("101", l, r);
        assert l = 0 and r = 2 severity failure;
        put_bv(bv, l, r);
        assert l = 5 and r = 3 severity failure;
        put_bv(up, l, r);
        assert l = 4 and r = 6 severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
