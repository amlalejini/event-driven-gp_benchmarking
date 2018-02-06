# Exploration script for signal GP benchmarking experiments.
library(ggplot2)
library(FSA)

######## Consensus data exploration #########
# Load data
ff_data <- read.csv("/Users/amlalejini/DataPlayground/signal-gp-benchmarking/consensus/final_fitness.csv")
fot_data <- as.data.frame(read.csv("/Users/amlalejini/DataPlayground/signal-gp-benchmarking/consensus/fitness_over_time.csv"))
fot_data$treatment <- as.factor(fot_data$treatment)
fot_data$run_id <- as.factor(fot_data$run_id)
fot_data$benchmark <- as.factor(fot_data$benchmark)
# Plot final fitness data.
plot(max_fitness ~ treatment, data=ff_data)

# Run an ANOVA
# Model: fitness_i = alpha + beta1*treatment_2 + beta2*treatment_3
# Build design matrix
model.matrix(~treatment, data=ff_data)
# Fit the model.
fit = lm(max_fitness ~ treatment, data=ff_data)

kw_fit <- kruskal.test(max_fitness ~ treatment, data=ff_data)
dt <- dunnTest(max_fitness~treatment, data=ff_data, method="bonferroni")





######## Changing Environment data exploration #########
ce_ff_data <-  read.csv("/Users/amlalejini/DataPlayground/signal-gp-benchmarking/changing_environment/mt_final_fitness.csv")

ce_ff_data_2 <- ce_ff_data[grep("ENV2$", ce_ff_data$treatment),]
ce_ff_data_2$treatment <- relevel(ce_ff_data_2$treatment, ref="ChgEnv_ED1_AS1_P125_ENV2")

ce_ff_data_4 <- ce_ff_data[grep("ENV4$", ce_ff_data$treatment),]
ce_ff_data_4$treatment <- relevel(ce_ff_data_4$treatment, ref="ChgEnv_ED1_AS1_P125_ENV4")

ce_ff_data_8 <- ce_ff_data[grep("ENV8$", ce_ff_data$treatment),]
ce_ff_data_8$treatment <- relevel(ce_ff_data_8$treatment, ref="ChgEnv_ED1_AS1_P125_ENV8")

ce_ff_data_16 <- ce_ff_data[grep("ENV16$", ce_ff_data$treatment),]
ce_ff_data_16$treatment <- relevel(ce_ff_data_16$treatment, ref="ChgEnv_ED1_AS1_P125_ENV16")

# ANOVA
fit_ce_2 <- lm(fitness ~ treatment, data=ce_ff_data_2)
fit_ce_4 <- lm(fitness ~ treatment, data=ce_ff_data_4)
fit_ce_8 <- lm(fitness ~ treatment, data=ce_ff_data_8)
fit_ce_16 <- lm(fitness ~ treatment, data=ce_ff_data_16)

# Effect size: Z/sqrt(N)

# Non-parametric ANOVA (Kruskal-Wallis)
kw_fit_ce_16 <- kruskal.test(fitness ~ treatment, data=ce_ff_data_16)
dt_ce_16 <- dunnTest(fitness~treatment, data=ce_ff_data_16, method="bonferroni")

kw_fit_ce_8 <- kruskal.test(fitness ~ treatment, data=ce_ff_data_8)
dt_ce_8 <- dunnTest(fitness~treatment, data=ce_ff_data_8, method="bonferroni")

kw_fit_ce_4 <- kruskal.test(fitness ~ treatment, data=ce_ff_data_4)
dt_ce_4 <- dunnTest(fitness~treatment, data=ce_ff_data_4, method="bonferroni")

kw_fit_ce_2 <- kruskal.test(fitness ~ treatment, data=ce_ff_data_2)
dt_ce_2 <- dunnTest(fitness~treatment, data=ce_ff_data_2, method="bonferroni")

######## Teasing apart the combined condition #########
ce_tff_data <-  read.csv("/Users/amlalejini/DataPlayground/signal-gp-benchmarking/changing_environment/teaser_final_fitness.csv")

ce_tff_data_2 <- ce_tff_data[grep("ENV2$", ce_tff_data$treatment),]
ce_tff_data_2$treatment <- relevel(ce_tff_data_2$treatment, ref="ChgEnv_ED1_AS1_P125_ENV2")

ce_tff_data_4 <- ce_tff_data[grep("ENV4$", ce_tff_data$treatment),]
ce_tff_data_4$treatment <- relevel(ce_tff_data_4$treatment, ref="ChgEnv_ED1_AS1_P125_ENV4")

ce_tff_data_8 <- ce_tff_data[grep("ENV8$", ce_tff_data$treatment),]
ce_tff_data_8$treatment <- relevel(ce_tff_data_8$treatment, ref="ChgEnv_ED1_AS1_P125_ENV8")

ce_tff_data_16 <- ce_tff_data[grep("ENV16$", ce_tff_data$treatment),]
ce_tff_data_16$treatment <- relevel(ce_tff_data_16$treatment, ref="ChgEnv_ED1_AS1_P125_ENV16")

# Non-parametric ANOVA (Kruskal-Wallis)
kw_ce_comb_16 <- kruskal.test(fitness ~ teaser, data=ce_tff_data_16)
dt_ce_comb_16 <- dunnTest(fitness~teaser, data=ce_tff_data_16, method="bonferroni")

kw_ce_comb_8 <- kruskal.test(fitness ~ teaser, data=ce_tff_data_8)
dt_ce_comb_8 <- dunnTest(fitness~teaser, data=ce_tff_data_8, method="bonferroni")

kw_ce_comb_4 <- kruskal.test(fitness ~ teaser, data=ce_tff_data_4)
dt_ce_comb_4 <- dunnTest(fitness~teaser, data=ce_tff_data_4, method="bonferroni")

kw_ce_comb_2 <- kruskal.test(fitness ~ teaser, data=ce_tff_data_2)
dt_ce_comb_2 <- dunnTest(fitness~teaser, data=ce_tff_data_2, method="bonferroni")

