-- DESCRIPTION: Verilator: VHDL binding that renames an open port
--
-- A component's port "c_bv_out : out bit_vector" is bound to the entity's
-- port "bv_out" by a configuration specification's port map (7.3.2.2).
-- Both are open, so each takes its subtype from what it is connected to
-- (6.5.6.3): the entity's port from the component's, the component's from
-- the instance's actual. Instances with actuals of different lengths and
-- directions each get their own (VESTS clifton-labs
-- unconstrained-array-example).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_bindopen_sub is
    port (bv_in : in bit_vector; bv_out : out bit_vector; len : out natural);
end entity;

architecture only of t_vhdl_bindopen_sub is
begin
    bv_out <= not bv_in;
    len <= bv_in'length;
end architecture;

entity t_vhdl_bindopen is
end entity;

architecture only of t_vhdl_bindopen is
    component sub_comp
        port (c_in : in bit_vector; c_out : out bit_vector; c_len : out natural);
    end component;

    for all : sub_comp
        use entity work.t_vhdl_bindopen_sub(only)
        port map (bv_in => c_in, bv_out => c_out, len => c_len);

    signal a4 : bit_vector(3 downto 0) := "0110";
    signal r4 : bit_vector(3 downto 0);
    signal a6 : bit_vector(1 to 6) := "110001";
    signal r6 : bit_vector(1 to 6);
    signal len4, len6 : natural;
begin
    u4 : component sub_comp port map (c_in => a4, c_out => r4, c_len => len4);
    u6 : component sub_comp port map (a6, r6, len6);

    process is
    begin
        wait for 1 ns;
        assert r4 = "1001" report "r4 " & to_string(r4) severity failure;
        assert r6 = "001110" report "r6 " & to_string(r6) severity failure;
        assert len4 = 4 and len6 = 6 report "lengths" severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;
end architecture;
