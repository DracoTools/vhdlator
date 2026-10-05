-- DESCRIPTION: Verilator: VHDL binding indication that gives an entity port
-- a value
--
-- "for all : nand2 use entity work.nand3 port map (a => in1, b => in2,
-- c => '1', y => result)" binds a two-input component to a three-input
-- entity and ties the third input to '1' (7.3.2.2): the entity's port takes
-- the value where the component has no port for it (VESTS ashenden
-- ch_13_fg_13_19).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_bindconst_nand3 is
    port (a, b, c : in bit; y : out bit);
end entity;

architecture behavioral of t_vhdl_bindconst_nand3 is
begin
    y <= not (a and b and c);
end architecture;

entity t_vhdl_bindconst is
end entity;

architecture test of t_vhdl_bindconst is
    component nand2 is
        port (in1, in2 : in bit; result : out bit);
    end component;

    for all : nand2
        use entity work.t_vhdl_bindconst_nand3(behavioral)
        port map (a => in1, b => in2, c => '1', y => result);

    component nand2z is
        port (in1, in2 : in bit; result : out bit);
    end component;

    for g0 : nand2z
        use entity work.t_vhdl_bindconst_nand3(behavioral)
        port map (a => in1, b => in2, c => '0', y => result);

    signal s1, s2, s3, s4 : bit := '0';
begin
    gate1 : component nand2 port map (in1 => s1, in2 => s2, result => s3);
    g0 : component nand2z port map (s1, s2, s4);

    process is
    begin
        s1 <= '1';
        s2 <= '1';
        wait for 1 ns;
        assert s3 = '0' report "nand3 with c = '1'" severity failure;
        assert s4 = '1' report "nand3 with c = '0'" severity failure;
        s2 <= '0';
        wait for 1 ns;
        assert s3 = '1' report "s3" severity failure;
        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
