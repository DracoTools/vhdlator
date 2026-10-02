-- DESCRIPTION: Verilator: VHDL aggregate as the default of an unconstrained parameter
--
-- The default is what a call that leaves the parameter out gets, with the
-- bounds the aggregate has.  A call that gives the parameter a value of
-- another length has nothing to do with it.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_dfltagg is
end entity;

architecture test of t_vhdl_dfltagg is
    type std8 is array (natural range <>) of bit_vector(7 downto 0);

    constant x : std8(0 to 3) := (0 => x"55", 1 => x"AA", 2 => x"55", 3 => x"AA");

    function f_make (pyld : std8 := (0 => x"7E")) return std8 is
        variable hdr : std8(0 to 1) := (others => (others => '0'));
    begin
        return hdr & pyld;
    end function;

    function total (data : in std8) return natural is
        variable n : natural := 0;
    begin
        for i in 0 to data'length - 1 loop
            for k in 7 downto 0 loop
                if data(i)(k) = '1' then
                    n := n + 1;
                end if;
            end loop;
        end loop;
        return n * 100 + data'length;
    end function;
begin

    process is
        variable one : std8(5 to 5) := (5 => x"0F");
    begin
        -- Given four elements: two of header and the four
        assert total(f_make(x)) = 1606 severity failure;
        -- Left out: the default's one element
        assert total(f_make) = 603 severity failure;
        -- Given one element, as long as the default is
        assert total(f_make(one)) = 403 severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
