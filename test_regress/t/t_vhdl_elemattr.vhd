-- DESCRIPTION: Verilator: VHDL attribute of an element's subtype, the element named out of range
--
-- "v2(5)'left" asks about the element subtype of v2, which every element
-- shares, and reads no element: an index outside the range is warned
-- about, not refused.  Elements whose own range is null still report it.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_elemattr is
end entity;

architecture test of t_vhdl_elemattr is
    type slv_1_t is array (natural range <>) of bit_vector;

    function get_elt_left (x : slv_1_t) return integer is
    begin
        return x(x'left)'left;
    end function;

    function get_elt_left_2 (x : slv_1_t) return integer is
    begin
        return x'element'left;
    end function;
begin

    p1 : process is
        variable v1 : slv_1_t(0 to 3)(5 to 6);
        variable v2 : slv_1_t(0 to 3)(6 to 5);
    begin
        assert v1(1)'left = 5 severity failure;
        assert v2(5)'left = 6 severity failure;
        assert v2(7)'length = 0 severity failure;
        assert get_elt_left(v1) = 5 severity failure;
        assert get_elt_left(v2) = 6 severity failure;
        assert get_elt_left_2(v1) = 5 severity failure;
        assert get_elt_left_2(v2) = 6 severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
